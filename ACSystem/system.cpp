#include "pch.h"
#include "system.h"

ACSystem::ACSystem(ACCom& com, ACLog& log, ACDbms& dbms, const std::vector<int64_t>& roomids) :
	_com(com), _log(log), _usr(),
	_capacity(0x3),
	_dlocker(), _acss(),  _acws(),
	_onrunning(false), _mcontroller(),
	_onstartup(false), _ccontroller()
{
	_usr.admin.handler = _com.CreateHandler(L"Admin");
	_usr.mgr.handler = _com.CreateHandler(L"Manager");
	_usr.rpt.handler = _com.CreateHandler(L"Reception");
	
	ACUsr& usr = _usr;
	std::for_each(roomids.begin(), roomids.end(), [&usr, &com, &dbms](int64_t roomid) {
		usr.rooms.push_back(new Room{ roomid, dbms });

		wchar_t rid[0xF];
		std::swprintf(rid, L"%I64d", roomid);
		usr.rooms.back()->handler = com.CreateHandler(rid);
	});

	_mcontroller = std::move(std::thread{ std::bind(&ACSystem::_master, this) });
	_log.Log(_log.Time().append(U("System has been started.")));
}

ACSystem::~ACSystem()
{
	_onstartup = false;
	if (_ccontroller.joinable())
		_ccontroller.join();

	_onrunning = false;
	if (_mcontroller.joinable())
		_mcontroller.join();

	_log.Log(_log.Time().append(U("System has been stopped.")));
}

void ACSystem::Wait()
{
	if (_mcontroller.joinable())
		_mcontroller.join();
}

void ACSystem::_master()
{
	ACMessage msg;
	try
	{
		_log.Log(_log.Time().append(U("System has been started correctly.")));

		_onrunning = true;
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
	catch (...)
	{
		_log.Log(_log.Time().append(U("System has been stopped incorrectly.")));

		_mcontroller.detach();
		_mcontroller = std::move(std::thread{ std::bind(&ACSystem::_master, this) });

		_log.Log(_log.Time().append(U("System has been restarted automatically.")));
	}
}

void ACSystem::_check()
{
	try
	{
		_log.Log(_log.Time().append(U("Wait-Queue Monitor has been started correctly.")));
		std::list<ACWObj*>::iterator toservice;
		std::list<ACSObj*>::iterator towait;

		_onstartup = true;
		while (_onstartup)
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
				time_t mindr = 0;
				time_t dr = 0;

				for (auto obj = _acss.begin(); obj != _acss.end(); ++obj)
				{
					dr = (*obj)->duration;
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
					_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue.")));

					delete *towait;
					*towait = nullptr;
					_acss.erase(towait);
				}

				_acss.push_back(new ACSObj{ (*toservice)->room, (*toservice)->tfanspeed });
				(*toservice)->room.SetFanspeed((*toservice)->tfanspeed, _usr.admin.frate[(*toservice)->tfanspeed]);

				std::swprintf(rid, U("%I64d"), (*toservice)->room.id);
				_log.Log(_log.Time().append(rid).append(U(" has been moved into Serivce-Queue.")));

				delete *toservice;
				*toservice = nullptr;
				_acws.erase(toservice);
			}

			for (auto obj = _acws.begin(); obj != _acws.end(); ++obj)
				(*obj)->duration -= rest;

			_dlocker.unlock();

			std::this_thread::sleep_for(std::chrono::seconds(rest));
		}
		_log.Log(_log.Time().append(U("Wait-Queue Monitor has been stopped correctly.")));
	}
	catch (...)
	{
		_log.Log(_log.Time().append(U("Wait-Queue Monitor has been stopped incorrectly.")));

		_ccontroller.detach();
		_ccontroller = std::move(std::thread{ std::bind(&ACSystem::_check, this) });

		_log.Log(_log.Time().append(U("Wait-Queue Monitor has been restarted automatically.")));
	}

}
