#include "pch.h"
#include "system.h"

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
		usr.rooms.push_back(new Room{ roomid });

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

void ACSystem::_room(const ACMessage& msg)
{
	if (_usr.admin.state != Admin::state_t::READY)
		return;

	try
	{
		switch (msg.type)
		{
		case ACMsgType::REQUESTON:
			_requeston(
				msg.body.at(U("RoomId")).as_integer(),
				(double_t)msg.body.at(U("CurrentRoomTemp")).as_integer()
			);
			break;
		case ACMsgType::REQUESTOFF:
			_requestoff(msg.body.at(U("RoomId")).as_integer());
			break;
		case ACMsgType::SETTEMP:
			_settemp(
				msg.body.at(U("RoomId")).as_integer(),
				(double_t)msg.body.at(U("TargetTemp")).as_integer()
			);
			break;
		case ACMsgType::SETFANSPEED:
			_setfanspeed(
				msg.body.at(U("RoomId")).as_integer(),
				(Room::speed_t)msg.body.at(U("FanSpeed")).as_integer()
			);
			break;
		case ACMsgType::TEMPNOTIFICATION:
			_notify(
				msg.body.at(U("RoomId")).as_integer(),
				(double_t)msg.body.at(U("CurrentRoomTemp")).as_integer()
			);
			break;
		default:
			break;
		}
	}
	catch (...)
	{

	}
}

void ACSystem::_requeston(int64_t id, double_t ctemp)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);

	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" Request on.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->invoice.Prepare(Invoice::opt_t::REQUESTON, (*room)->latest);
		(*room)->rquestcnt = 1;

		json::value msg;
		if ((int64_t)_acss.size() > _capacity)
		{
			_wlocker.lock();
			_acws.push_back(new ACWObj{ *(*room), _usr.admin.deffanspeed, 120 });
			_wlocker.unlock();
			(*room)->inservice = false;

			msg[U("State")] = json::value::string(U("wait"));
			_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::WAIT, msg });

			_log.Log(_log.Time().append(rid).append(U(" Request on has been delayed.")));
		}
		else
		{
			_slocker.lock();
			_acss.push_back(new ACSObj{ *(*room), _usr.admin.deffanspeed });
			_acss.back()->Serve(ctemp);
			_slocker.unlock();
			(*room)->inservice = true;

			msg[U("State")] = json::value::string(U("ok"));
			if ((*room)->mode == Room::mode_t::HOT)
				msg[U("Mode")] = json::value::string(U("HOT"));
			else if ((*room)->mode == Room::mode_t::COOL)
				msg[U("Mode")] = json::value::string(U("COOL"));
			msg[U("TargetTemp")] = json::value::number((int64_t)(*room)->GetTargetTemp());
			msg[U("FeeRate")] = json::value::number(_usr.admin.frate[(*room)->GetFanspeed()]);
			msg[U("Fee")] = json::value::number((*room)->GetTotalfee());

			_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::REQUESTON, msg });

			_log.Log(_log.Time().append(rid).append(U(" Request on has been handled.")));
		}

		(*room)->rponsecnt = 1;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(L"Room does not exist."));
	}
}

void ACSystem::_requestoff(int64_t id)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);

	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" Request off.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->invoice.Prepare(Invoice::opt_t::REQUESTOFF, (*room)->latest);
		(*room)->rquestcnt++;

		_slocker.lock();
		_wlocker.lock();

		auto sobj = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
			return (*room)->id == (*obj).room.id;
		});

		if (sobj != _acss.end())
		{
			delete *sobj;
			*sobj = nullptr;
			_acss.erase(sobj);
		}
		else
		{
			auto wobj = std::find_if(_acws.begin(), _acws.end(), [&room](const ACWObj* obj) {
				return (*room)->id == (*obj).room.id;
			});
			if (wobj != _acws.end())
			{
				delete *wobj;
				*wobj = nullptr;
				_acws.erase(wobj);
			}
		}

		_wlocker.unlock();
		_slocker.unlock();

		json::value msg;
		msg[U("Fee")] = json::value::number((*room)->GetTotalfee());
		msg[U("Duration")] = json::value::number((int64_t)(*room)->GetPeriod());
		_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::REQUESTOFF, msg });

		(*room)->rponsecnt++;

		(*room)->Reset();
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U( "Room does not exist.")));
	}
}

void ACSystem::_settemp(int64_t id, double_t ttemp)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);
	wchar_t tt[0xF];
	wsprintf(tt, U("%lf"), ttemp);

	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" Set target temperature as ")).append(tt).append(U("¡æ.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->invoice.Prepare(Invoice::opt_t::SETTEMP, (*room)->latest);
		(*room)->rquestcnt++;

		json::value msg;
		if (ttemp >= _usr.admin.ltemp && ttemp <= _usr.admin.htemp)
		{
			(*room)->SetTargetTemp(ttemp);
			msg[U("isOk")] = json::value::string(U("True"));
		}
		else
		{
			msg[U("isOk")] = json::value::string(U("False"));
		}
		_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::SETTEMP, msg });

		(*room)->rponsecnt++;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}
}

void ACSystem::_setfanspeed(int64_t id, Room::speed_t fanspeed)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);
	wchar_t fs[0xF];
	wsprintf(fs, U("%lf"), fanspeed);

	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" Set fanspeed as ")).append(fs).append(U(".")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->invoice.Prepare(Invoice::opt_t::SETFANSPEED, std::time(nullptr));
		(*room)->rquestcnt++;

		_slocker.lock();
		_wlocker.lock();

		auto sobj = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
			return (*room)->id == (*obj).room.id;
		});

		if (sobj != _acss.end())
		{
			(*sobj)->fanspeed = fanspeed;
			(*sobj)->room.SetFanspeed(fanspeed, _usr.admin.frate[fanspeed]);

			_log.Log(_log.Time().append(rid).append(U(" is in Service-Queue.")));
		}
		else
		{
			auto wobj = std::find_if(_acws.begin(), _acws.end(), [&room](const ACWObj* obj) {
				return (*room)->id == (*obj).room.id;
			});

			Room::speed_t fs = (*room)->GetFanspeed();
			if (fanspeed > fs)
			{
				std::list<std::list<ACSObj*>::iterator> prep;
				Room::speed_t minfs{ Room::speed_t::LMT };
				for (auto elem = _acss.begin(); elem != _acss.end(); ++elem)
				{
					if ((*elem)->fanspeed < fanspeed && (*elem)->fanspeed <= minfs)
					{
						prep.push_back(elem);
						minfs = (*elem)->fanspeed;
					}
				}

				if (prep.size() == 0)
				{
					if (wobj != _acws.end())
					{
						(*wobj)->tfanspeed = fanspeed;
						_log.Log(_log.Time().append(rid).append(U(" still stays in Wait-Queue.")));
					}
					else
					{
						_acws.push_back(new ACWObj{ *(*room), fanspeed, 120 });
						_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue.")));
					}
				}
				else
				{
					std::list<std::list<ACSObj*>::iterator> maximal;
					time_t longest = 0;
					for (auto elem = prep.begin(); elem != prep.end(); ++elem)
					{
						time_t dr = (*(*elem))->GetDuration();
						if ((*(*elem))->fanspeed == minfs
							&& dr > longest)
						{
							maximal.push_back(*elem);
							longest = dr;
						}
					}

					auto release = maximal.back();
					Room& wait = (*release)->room;

					delete *release;
					*release = nullptr;
					_acss.erase(release);
					_acws.push_back(new ACWObj{ wait, (*room)->GetFanspeed(), 120 });

					if (wobj != _acws.end())
					{
						delete *wobj;
						*wobj = nullptr;
						_acws.erase(wobj);
					}

					_acss.push_back(new ACSObj{ *(*room), fanspeed });
					(*room)->SetFanspeed(fanspeed, _usr.admin.frate[fanspeed]);

					_log.Log(_log.Time().append(rid).append(U(" has been  moved into Serice-Queue.")));

					wsprintf(rid, U("%ld"), wait.id);
					_log.Log(_log.Time().append(rid).append(U(" has been  moved into Wait-Queue.")));
				}
			}
			else if (fanspeed == fs)
			{
				if (wobj != _acws.end())
				{
					(*wobj)->tfanspeed = fanspeed;
					(*wobj)->duration = 120;

					_log.Log(_log.Time().append(rid).append(U(" is in Wait-Queue.")));
				}
				else
				{
					_acws.push_back(new ACWObj{ *(*room), fanspeed, 120 });

					_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue.")));
				}
			}
			else
			{
				if (wobj != _acws.end())
				{
					(*wobj)->tfanspeed = fanspeed;
					(*wobj)->duration = INT64_MAX;

					_log.Log(_log.Time().append(rid).append(U(" is in Wait-Queue until Service-Queue is available.")));
				}
				else
				{
					_acws.push_back(new ACWObj{ *(*room), fanspeed, INT64_MAX });

					_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue until Service-Queue is available.")));
				}
			}
		}

		_wlocker.unlock();
		_slocker.unlock();

		json::value msg;
		msg[U("FeeRate")] = json::value::number(_usr.admin.frate[(*room)->GetFanspeed()]);
		_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::SETFANSPEED, msg });

		(*room)->rponsecnt++;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}
}

void ACSystem::_fetchfee(int64_t id)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);

	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" are fectching the fee.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->rquestcnt++;

		json::value msg;
		msg[U("FeeRate")] = json::value::number(_usr.admin.frate[(*room)->GetFanspeed()]);
		msg[U("Fee")] = json::value::number((*room)->GetTotalfee());
		_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::FETCHFEE, msg });
		(*room)->rponsecnt++;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}
}

void ACSystem::_notify(int64_t id, double_t ctemp)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);
	wchar_t ct[0xF];
	wsprintf(ct, U("%lf"), ctemp);

	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" notifies current temperature as ")).append(ct).append(U("¡æ.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->rquestcnt++;

		(*room)->SetCurrentTemp(ctemp);

		int64_t state = 0;
		double_t ttemp = (*room)->GetTargetTemp();
		if (std::fabs(ctemp - ttemp) < 1.0)
		{
			_log.Log(_log.Time().append(rid).append(U(" reaches the target temperature.")));

			state = 3;
			_slocker.lock();
			auto release = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
				return (*room)->id == obj->room.id;
			});
			if (release != _acss.end())
			{
				delete *release;
				*release = nullptr;
				_acss.erase(release);

				_wlocker.lock();
				if (_acws.size() > 0)
				{
					std::list<std::list<ACWObj*>::iterator> minimal;
					time_t mindr = INT_MAX;
					for (auto elem = _acws.begin(); elem != _acws.end(); ++elem)
					{
						if ((*elem)->duration < mindr)
						{
							mindr = (*elem)->duration;
							minimal.push_back(elem);
						}
					}

					auto release = minimal.back();
					Room& wait = (*release)->room;

					_acss.push_back(new ACSObj{ wait, (*release)->tfanspeed });
					wait.SetFanspeed((*release)->tfanspeed, _usr.admin.frate[(*release)->tfanspeed]);

					delete *release;
					*release = nullptr;

					wsprintf(rid, U("%ld"), wait.id);
					_log.Log(_log.Time().append(rid).append(U(" has been moved into Service-Queue.")));
				}
				_wlocker.unlock();
			}
			_slocker.unlock();
		}
		else
		{
			_slocker.lock();
			auto index = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
				return (*room)->id == obj->room.id;
			});
			if (index == _acss.end())
				state = 2;
			else
				state = 1;
			_slocker.unlock();
		}

		json::value msg;
		msg[U("ACState")] = json::value::number(state);
		_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::TEMPNOTIFICATION, msg });

		(*room)->rponsecnt++;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}
}

void ACSystem::_admin(const ACMessage& msg)
{
	Room::mode_t mode;
	try
	{
		switch (msg.type)
		{
		case ACMsgType::POWERON:
			_poweron();
			break;
		case ACMsgType::SETPARAM:
			if (msg.body.at(U("State")).as_string().compare(U("HOT")) == 0)
				mode = Room::mode_t::HOT;
			else if (msg.body.at(U("State")).as_string().compare(U("COOL")) == 0)
				mode = Room::mode_t::COOL;
			_setparam(
				mode,
				(double_t)msg.body.at(U("TempHighLimit")).as_integer(),
				(double_t)msg.body.at(U("TempLowLimit")).as_integer(),
				(double_t)msg.body.at(U("DefaultTargetTemp")).as_integer(),
				msg.body.at(U("FeeRateH")).as_double(),
				msg.body.at(U("FeeRateM")).as_double(),
				msg.body.at(U("FeeRateL")).as_double()
			);
			break;
		case ACMsgType::STARTUP:
			_startup();
			break;
		case ACMsgType::SHUTDOWN:
			_shutdown();
			break;
		case ACMsgType::MONITOR:
			_monitor(msg.body.at(U("RoomId")).as_integer());
			break;
		default:
			break;
		}
	}
	catch (...)
	{

	}
}

void ACSystem::_poweron()
{
	_log.Log(_log.Time().append(U("Administrator requests to power on the system.")));
	_usr.admin.state = Admin::state_t::SET;

	_usr.admin.latest = std::time(nullptr);
	_usr.admin.opt = Admin::opt_t::POWERON;
	_usr.admin.start = _usr.admin.latest;

	_usr.admin.rquestcnt = 1;

	json::value msg;
	msg[U("state")] = json::value::string(U("SetMode"));
	_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::POWERON, msg });
	
	_usr.admin.rponsecnt = 1;
}

void ACSystem::_setparam(Room::mode_t mode, double_t ht, double_t lt, double_t dt, double_t hf, double_t mf, double_t lf)
{
	_usr.admin.rquestcnt++;

	_log.Log(_log.Time().append(U("Administrator requests to set the system.")));
	json::value msg;
	if (_usr.admin.state != Admin::state_t::SET
		|| dt < lt || dt > ht || lt > ht)
	{
		msg[U("isOk")] = json::value::string(U("False"));
		_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::SETPARAM, msg });
		_log.Log(_log.Time().append(U("Set params request has been refuesed.")));
	}
	else
	{
		_usr.admin.latest = std::time(nullptr);
		_usr.admin.opt = Admin::opt_t::SETPARAM;

		_usr.admin.defmode = mode;
		_usr.admin.htemp = ht;
		_usr.admin.ltemp = lt;
		_usr.admin.deftemp = dt;
		_usr.admin.frate[Room::speed_t::LOW] = lf;
		_usr.admin.frate[Room::speed_t::MID] = mf;
		_usr.admin.frate[Room::speed_t::HGH] = hf;
		_usr.admin.deffanspeed = Room::speed_t::LOW;

		msg[U("isOk")] = json::value::string(U("True"));
		_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::SETPARAM, msg });
		_log.Log(_log.Time().append(U("Set params correctly.")));
	}

	_usr.admin.rponsecnt++;
}

void ACSystem::_startup()
{
	_usr.admin.rquestcnt++;

	_log.Log(_log.Time().append(U("Administrator requests to start the system.")));
	json::value msg;
	if (_usr.admin.state != Admin::state_t::SET)
	{
		msg[U("state")] = json::value::string(U("set"));
		_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::STARTUP, msg });
		_log.Log(_log.Time().append(U("Failed to start the system.")));
	}
	else
	{
		_usr.admin.latest = std::time(nullptr);
		_usr.admin.opt = Admin::opt_t::STARTUP;
		_usr.admin.state = Admin::state_t::READY;

		_ccontroller = std::move(std::thread{ std::bind(&ACSystem::_check, this) });

		msg[U("state")] = json::value::string(U("ready"));
		_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::STARTUP, msg });
		_log.Log(_log.Time().append(U("Start the system correctly.")));
	}

	_usr.admin.rponsecnt++;
}

void ACSystem::_monitor(int64_t roomid)
{
	_usr.admin.rquestcnt++;

	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), roomid);

	std::list<Room*>::iterator room = _usr.rooms.end();
	try
	{
		_log.Log(_log.Time().append(U("Administrator needs ")).append(rid).append(U(".")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [roomid](const Room* cur) {
			return cur->id == roomid;
		});

		time_t duration = 0;
		_slocker.lock();
		auto sobj = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
			return (*room)->id == obj->room.id;
		});
		if (sobj != _acss.end())
			duration = (*sobj)->GetDuration();
		_slocker.unlock();
		Room::state_t state = (*room)->GetState();
		double_t ctemp = (*room)->GetCurrentTemp();
		double_t ttemp = (*room)->GetTargetTemp();
		Room::speed_t fspeed = (*room)->GetFanspeed();
		double_t frate = _usr.admin.frate[fspeed];
		double_t fee = (*room)->GetTotalfee();

		// TODO
		json::value msg;
		switch (state)
		{
		case Room::state_t::SERVICE:
			msg[U("state")] = json::value::string(U("Service"));
			break;
		case Room::state_t::STOPPED:
			msg[U("state")] = json::value::string(U("Stopped"));
			break;
		case Room::state_t::SUSPEND:
			msg[U("state")] = json::value::string(U("Suspend"));
			break;
		default:
			break;
		}
		msg[U("CurrentTemp")] = json::value::number((int64_t)ctemp);
		msg[U("TargetTemp")] = json::value::number((int64_t)ttemp);
		msg[U("Fan")] = json::value::number((int64_t)fspeed);
		msg[U("FeeRate")] = json::value::number(frate);
		msg[U("Fee")] = json::value::number(fee);
		msg[U("Duration")] = json::value::number((int64_t)duration);
		_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::MONITOR, msg });

		_usr.admin.rponsecnt++;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}
}

void ACSystem::_shutdown()
{
	// TODO
}

void ACSystem::_mgr(const ACMessage& msg)
{
}

void ACSystem::_fetchreport(int64_t roomid, Mgr::rtype_t rt, time_t head)
{
}

void ACSystem::_rpt(const ACMessage& msg)
{
}

void ACSystem::_fetchbill(int64_t roomid, time_t din, time_t dout)
{
}

void ACSystem::_fetchinvoice(int64_t roomid, time_t din, time_t dout)
{
}
