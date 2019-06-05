#include "pch.h"
#include "system.h"

/// <summary>
/// @Description : Initialize resources
/// @Author : Chengxin
/// @Date : 2019/5/27
/// </summary>
/// <param name="com">Web Http Communicator</param>
/// <param name="log">Local Logger</param>
/// <param name="dbms">Local DBMS</param>
/// <param name="roomids">ID of rooms,eg.201,202,203</param>
ACSystem::ACSystem(ACCom& com, ACLog& log, ACDbms& dbms, const std::vector<int64_t>& roomids) :
	_com(com), _log(log), _usr(),
	_capacity(0x3), // default capacity is 3
	_dlocker(), _acss(), _acws(), _watcher(),
	_onrunning(false), _ccontroller(),
	_acontroller()
{
	_usr.admin.handler = _com.CreateHandler(L"Admin");
	_usr.mgr.handler = _com.CreateHandler(L"Manager");
	_usr.rpt.handler = _com.CreateHandler(L"Reception");

	// initialize the communicator, distributing handlers for 
	// rooms, administrator, manager and receptionist
	ACUsr& usr = _usr;
	std::for_each(roomids.begin(), roomids.end(), [&usr, &com, &dbms, &log](int64_t roomid) {
		usr.rooms.push_back(new Room{ roomid, dbms, log });

		wchar_t rid[0xF];
		std::swprintf(rid, L"%I64d", roomid);
		usr.rooms.back()->handler = com.CreateHandler(rid);
		usr.rooms.back()->Run();
	});
}

/// <summary>
/// @Description : Release resources
/// @Author : Chengxin
/// @Date : 2019/5/27
/// </summary>
ACSystem::~ACSystem()
{
	// waiting for controller handling over
	if (_ccontroller.joinable())
		_ccontroller.join();

	if (_acontroller.joinable())
		_acontroller.join();

	// waiting for outer message handling over
	_onrunning = false;
	for (auto& room : _usr.rooms)
		delete room;

	_log.Log(_log.Time().append(U("System has been stopped.")));
}

/// <summary>
/// @Description : Run the service
/// @Author : Chengxin
/// @Date : 2019/5/27
/// </summary>
void ACSystem::Start()
{
	// start the master thread for handling message from web communicator
	_onrunning = true;
	_master();
	_log.Log(_log.Time().append(U("System has been started.")));
}

/// <summary>
/// @Description : Fetch web message and handle it
/// @Author : Chengxin
/// @Date : 2019/5/27
/// </summary>
void ACSystem::_master()
{
	ACMessage msg;
	_log.Log(_log.Time().append(U("System has been started correctly.")));

	_ccontroller = std::move(std::thread{ std::bind(&ACSystem::_check, this) });
	_acontroller = std::move(std::thread{ std::bind(&ACSystem::_alive, this) });

	while (_onrunning)
	{
		msg = _com.PullMessage();
		switch (msg.type)
		{
		case ACMsgType::REQUESTON:
		case ACMsgType::REQUESTOFF:
		case ACMsgType::SETTEMP:
		case ACMsgType::SETFANSPEED:
		case ACMsgType::FETCHFEE:
		case ACMsgType::TEMPNOTIFICATION:
			_room(msg);
			break;

		case ACMsgType::POWERON:
		case ACMsgType::SETPARAM:
		case ACMsgType::STARTUP:
		case ACMsgType::SHUTDOWN:
		case ACMsgType::MONITOR:
			_admin(msg);
			break;

		case ACMsgType::FETCHBILL:
		case ACMsgType::FETCHINVOICE:
			_rpt(msg);
			break;

		case ACMsgType::FETCHREPORT:
			_mgr(msg);
			break;

		default:
			break;
		}
	}
	_log.Log(_log.Time().append(U("System has been stopped correctly.")));
}

void ACSystem::_check()
{
	_log.Log(_log.Time().append(U("Wait-Queue Monitor has been started correctly.")));
	std::list<ACWObj*>::iterator toservice;
	std::list<ACSObj*>::iterator towait;
	while (_onrunning)
	{
		time_t rest = INT64_MAX;

		_dlocker.lock();
		bool valid = false;
		for (auto obj = _acws.begin(); obj != _acws.end(); ++obj)
		{
			rest = std::min<time_t>(rest, (*obj)->duration);
			if ((*obj)->duration == 0)
			{
				toservice = obj;
				valid = true;
				break;
			}
		}

		if (valid)
		{
			wchar_t rid[0xF];
			time_t mindr = -1;
			time_t dr = 0;

			towait = _acss.end();
			time_t curtime = std::time(nullptr);
			for (auto obj = _acss.begin(); obj != _acss.end(); ++obj)
			{
				dr = curtime - (*obj)->timestamp;
				if (dr > mindr)
				{
					mindr = dr;
					towait = obj;
				}
			}

			if (towait != _acss.end())
			{
				_acws.push_back(new ACWObj{ (*towait)->room, (*towait)->room.GetFanspeed(), 120 });

				std::swprintf(rid, U("%I64d"), (*towait)->room.id);
				_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue cause timed out.")));

				delete *towait;
				*towait = nullptr;
				_acss.erase(towait);
			}

			_acss.push_back(new ACSObj{ (*toservice)->room, (*toservice)->tfanspeed, _usr.admin.frate[(*toservice)->tfanspeed] });
			(*toservice)->room.SetFanspeed((*toservice)->tfanspeed, _usr.admin.frate[(*toservice)->tfanspeed]);

			std::swprintf(rid, U("%I64d"), (*toservice)->room.id);
			_log.Log(_log.Time().append(rid).append(U(" has been moved into Serivce-Queue cause timed out.")));

			delete *toservice;
			*toservice = nullptr;
			_acws.erase(toservice);
		}

		if (rest != INT64_MAX)
		{
			for (auto obj = _acws.begin(); obj != _acws.end(); ++obj)
			{
				if ((*obj)->duration != INT64_MAX)
				{
					(*obj)->duration -= rest;
				}
			}
		}
		_dlocker.unlock();

		if (rest != INT64_MAX)
		{
			std::this_thread::sleep_for(std::chrono::seconds(rest));
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
	_log.Log(_log.Time().append(U("Wait-Queue Monitor has been stopped correctly.")));
}

void ACSystem::_alive()
{
	wchar_t rid[0xF];
	std::vector<int64_t> toremove;
	while (_onrunning)
	{
		toremove.clear();
		_dlocker.lock();
		for (auto& elem : _watcher)
		{
			if (elem.second == false)
			{
				auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&elem](const Room* cur) {
					return elem.first == cur->id;
				});
				if (room != _usr.rooms.end())
				{
					auto sobj = std::find_if(_acss.begin(), _acss.end(), [&elem](const ACSObj* obj) {
						return elem.first == obj->room.id;
					});
					if (sobj != _acss.end())
					{
						delete *sobj;
						*sobj = nullptr;
						_acss.erase(sobj);
					}

					auto wobj = std::find_if(_acws.begin(), _acws.end(), [&elem](const ACWObj* obj) {
						return elem.first == obj->room.id;
					});
					if (wobj != _acws.end())
					{
						delete *wobj;
						*wobj = nullptr;
						_acws.erase(wobj);
					}
					(*room)->Off(true);
					toremove.push_back((*room)->id);

					std::swprintf(rid, U("%I64d"), (*room)->id);
					_log.Log(_log.Time().append(rid).append(U(" Offline.")));
				}
			}
			elem.second = false;
		}
		for (const auto& elem : toremove)
			_watcher.erase(elem);
		_dlocker.unlock();

		std::this_thread::sleep_for(std::chrono::seconds(15));
	}

	_log.Log(_log.Time().append(U("Alive thread Crashed.")));
}