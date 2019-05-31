#include "pch.h"
#include "system.h"

ACSystem::ACSystem(ACCom& com, ACLog& log, ACDbms& dbms, const std::initializer_list<int64_t>& roomids) :
	_com(com), _log(log),
	_capacity(0x3),
	_mcontroller()
{
	_usr.admin.handler = _com.CreateHandler(L"Admin");
	_usr.mgr.handler = _com.CreateHandler(L"Manager");
	_usr.rpt.handler = _com.CreateHandler(L"Reception");
	
	ACUsr& usr = _usr;
	std::for_each(roomids.begin(), roomids.end(), [&usr, &com, &dbms](int64_t roomid) {
		usr.rooms.push_back(new Room{ roomid, dbms });

		wchar_t rid[0xF];
		wsprintf(rid, L"%ld", roomid);
		usr.rooms.back()->handler = com.CreateHandler(rid);
	});

	_mcontroller = std::move(std::thread{ std::bind(&ACSystem::_master, this) });

	_log.Log(_log.Time().append(U("System has been started.")));
}

ACSystem::~ACSystem()
{
	if (_mcontroller.joinable())
		_mcontroller.join();

	_log.Log(_log.Time().append(U("System has been stopped.")));
}

void ACSystem::_master()
{
	_log.Log(_log.Time().append(U("Communicator has been started correctly.")));
	ACMessage msg;
	while (true)
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
	_log.Log(_log.Time().append(U("Communicator has been stopped correctly.")));
}

void ACSystem::_check()
{
	_log.Log(_log.Time().append(U("Wait-Queue Monitor has been started correctly.")));
	std::list<ACWObj*>::iterator readys;
	std::list<ACSObj*>::iterator release;
	while (_onstartup)
	{
		time_t rest = INT64_MAX;

		_wlocker.lock();
		bool valid = false;
		for (auto obj = _acws.begin(); obj != _acws.end(); ++obj)
		{
			rest = std::min<time_t>(rest, (*obj)->duration);
			if ((*obj)->duration == 0)
			{
				readys = obj;
				valid = true;
				break;
			}
		}

		if (valid)
		{
			wchar_t rid[0xF];
			time_t mindr = 0;
			time_t dr = 0;

			_slocker.lock();
			for (auto obj = _acss.begin(); obj != _acss.end(); ++obj)
			{
				dr = (*obj)->GetDuration();
				if (dr > mindr)
				{
					mindr = dr;
					release = obj;
				}
			}

			if (release != _acss.end())
			{
				Room& wait = (*release)->room;
				delete *release;
				*release = nullptr;
				_acss.erase(release);

				_wlocker.lock();
				_acws.push_back(new ACWObj{ wait, wait.GetFanspeed(), 120 });
				_wlocker.unlock();

				wsprintf(rid, U("%ld"), wait.id);
				_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue.")));
			}

			Room& service = (*readys)->room;
			Room::speed_t tfs = (*readys)->tfanspeed;
			delete *readys;
			*readys = nullptr;
			_acws.erase(readys);

			_acss.push_back(new ACSObj{ service, tfs });
			service.SetFanspeed(tfs, _usr.admin.frate[tfs]);

			wsprintf(rid, U("%ld"), service.id);
			_log.Log(_log.Time().append(rid).append(U(" has been moved into Serivce-Queue.")));
			_slocker.unlock();
		}

		for (auto obj = _acws.begin(); obj != _acws.end(); ++obj)
			(*obj)->duration -= rest;
		_wlocker.unlock();

		std::this_thread::sleep_for(std::chrono::seconds(rest));
	}
	_log.Log(_log.Time().append(U("Wait-Queue Monitor has been stopped correctly.")));
}
