#include "pch.h"
#include "system.h"
#include <Windows.h>
#include <regex>

ACSystem::ACSystem(ACCom& com, ACLog& log, const std::initializer_list<int64_t>& roomids) :
	_com(com), _log(log),
	_capacity(0x3),
	_mcontroller()
{
	_usr.admin.handler = _com.CreateHandler(L"Admin");
	_usr.mgr.handler = _com.CreateHandler(L"Manager");
	_usr.rpt.handler = _com.CreateHandler(L"Reception");
	
	ACUsr& usr = _usr;
	std::for_each(roomids.begin(), roomids.end(), [&usr, &com](int64_t roomid) {
		usr.rooms.push_back(Room{ roomid });

		WCHAR rid[0xF];
		wsprintf(rid, L"%lld", roomid);
		usr.rooms.back().id = com.CreateHandler(rid);
	});

	_mcontroller = std::move(std::thread{ std::bind(&ACSystem::_master, this) });
}

ACSystem::~ACSystem()
{
	if (_mcontroller.joinable())
		_mcontroller.join();
}

//SYSTEMTIME st = { 0 };
//GetLocalTime(&st);
//WCHAR ct[0xFF];
//wsprintf(ct, L"%d-%02d-%02d %02d:%02d:%02d ",
//	st.wYear,
//	st.wMonth,
//	st.wDay,
//	st.wHour,
//	st.wMinute,
//	st.wSecond
//);
//_log.Log(std::wstring{ ct }.append(L"Start server failed."));

void ACSystem::_master()
{
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
		case ACMsgType::TEMPNOTIFICATION:
			_room(msg);
			break;

		case ACMsgType::POWERON:
		case ACMsgType::POWEROFF:
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
}

void ACSystem::_check()
{
	std::list<ACWObj*>::iterator readys;
	std::list<ACSObj*>::iterator release;
	while (_poweron)
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
			delete *readys;
			*readys = nullptr;
			_acws.erase(readys);
		}

		for (auto obj = _acws.begin(); obj != _acws.end(); ++obj)
			(*obj)->duration -= rest;
		_wlocker.unlock();

		if (valid)
		{
			_slocker.lock();
			time_t mindr = 0;
			time_t dr = 0;
			for (auto obj = _acss.begin(); obj != _acss.end(); ++obj)
			{
				dr = (*obj)->GetDuration();
				if (dr > mindr)
				{
					mindr = dr;
					release = obj;
				}
			}
			delete *release;
			*release = nullptr;
			_acss.erase(release);

			_acss.push_back(new ACSObj{ (*readys)->room, (*readys)->tfanspeed });
			_slocker.unlock();
		}

		std::this_thread::sleep_for(std::chrono::seconds(rest));
	}
}

void ACSystem::_room(const ACMessage& msg)
{
	if (_usr.admin.state != Admin::state_t::READY)
		return;

	int64_t id{0};
	double_t key{0.0};
	switch (msg.type)
	{
	case ACMsgType::REQUESTON:
		std::swscanf(msg.info.c_str(), L"RoomId=%lld&CurrentRoomTemp=%lf", &id, &key);
		_requeston(id, key);
		break;
	case ACMsgType::REQUESTOFF:
		std::swscanf(msg.info.c_str(), L"RoomId=%lld", &id);
		_requestoff(id);
		break;
	case ACMsgType::SETTEMP:
		std::swscanf(msg.info.c_str(), L"RoomId=%lld&TargetTemp=%lf", &id, &key);
		_settemp(id, key);
		break;
	case ACMsgType::SETFANSPEED:
		std::swscanf(msg.info.c_str(), L"RoomId=%lld&FanSpeed=%lf", &id, &key);
		_setfanspeed(id, key);
		break;
	case ACMsgType::TEMPNOTIFICATION:
		std::swscanf(msg.info.c_str(), L"RoomId=%lld&CurrentRoomTemp=%lf", &id, &key);
		_notification(id, key);
		break;
	default:
		break;
	}
}

void ACSystem::_requeston(int64_t id, double_t ctemp)
{
	std::list<Room>::iterator room;
	try
	{
		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room& cur) {
			return cur.id == id;
		});
	}
	catch (...)
	{
		SYSTEMTIME st = { 0 };
		GetLocalTime(&st);
		WCHAR ct[0xFF];
		wsprintf(ct, L"%d-%02d-%02d %02d:%02d:%02d ",
			st.wYear,
			st.wMonth,
			st.wDay,
			st.wHour,
			st.wMinute,
			st.wSecond
		);
		_log.Log(std::wstring{ ct }.append(L"Room does not exist."));
	}

	room->latest = std::time(nullptr);
	room->rquestcnt = 1;

	wchar_t msg[0xFF];
	if ((int64_t)_acss.size() > _capacity)
	{
		_wlocker.lock();
		_acws.push_back(new ACWObj{ *room, _usr.admin.deffanspeed, 120 });
		_wlocker.unlock();
		room->inservice = false;

		_com.PushMessage(ACMessage{ room->handler, ACMsgType::WAIT, L"wait" });
	}
	else
	{
		_slocker.lock();
		_acss.push_back(new ACSObj{ *room, _usr.admin.deffanspeed });
		_acss.back()->Serve(ctemp);
		_slocker.unlock();
		room->inservice = true;

		switch (room->mode)
		{
		case Room::mode_t::HOT:
			wsprintf(msg, L"State=ok\nMode=HOT\n%TargetTemp=lld\n%Feerate=lf%\n%Fee=lf\n",
				room->ttemp, room->GetFeerate(), room->GetTotalfee()
			);
			break;
		case Room::mode_t::COOL:
			wsprintf(msg, L"State=ok\nMode=HOT\n%TargetTemp=lld\n%Feerate=lf%\n%Fee=lf\n",
				room->ttemp, room->GetFeerate(), room->GetTotalfee()
			);
			break;
		default:
			break;
		}
		_com.PushMessage(ACMessage{ room->handler, ACMsgType::REQUESTON, msg });
	}

	room->rponsecnt = 1;
}

void ACSystem::_requestoff(int64_t id)
{
}

void ACSystem::_settemp(int64_t id, double_t ttemp)
{
}

void ACSystem::_setfanspeed(int64_t id, double_t fanspeed)
{
}

void ACSystem::_notification(int64_t id, double_t ctemp)
{
}

void ACSystem::_admin(const ACMessage & msg)
{
}

void ACSystem::_mgr(const ACMessage & msg)
{
}

void ACSystem::_rpt(const ACMessage & msg)
{
	if (_usr.admin.state != Admin::state_t::READY)
		return;
}