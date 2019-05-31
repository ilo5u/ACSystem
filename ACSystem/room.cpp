#include "pch.h"
#include "system.h"

void ACSystem::_room(const ACMessage& msg)
{
	try
	{
		if (_usr.admin.state != Admin::state_t::READY)
			throw std::wstring{ U("System was off.") };

		switch (msg.type)
		{
		case ACMsgType::REQUESTON:
			_requeston(
				msg.body.at(U("RoomId")).as_integer(),
				msg.body.at(U("CurrentRoomTemp")).as_double()
			);
			break;
		case ACMsgType::REQUESTOFF:
			_requestoff(msg.body.at(U("RoomId")).as_integer());
			break;
		case ACMsgType::SETTEMP:
			_settemp(
				msg.body.at(U("RoomId")).as_integer(),
				msg.body.at(U("TargetTemp")).as_double()
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
				msg.body.at(U("CurrentRoomTemp")).as_double()
			);
			break;
		default:
			break;
		}
	}
	catch (const std::wstring& e)
	{
		_log.Log(_log.Time().append(e));
	}
	catch (...)
	{
		_log.Log(_log.Time().append(U(" Room handling method crashed.")));
	}
}

void ACSystem::_requeston(int64_t id, double_t ctemp)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);

	json::value msg;
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
		(*room)->inservice = true;
		if ((int64_t)_acss.size() > _capacity)
		{
			_wlocker.lock();
			_acws.push_back(new ACWObj{ *(*room), _usr.admin.deffanspeed, 120 });
			_wlocker.unlock();

			msg[U("State")] = json::value::string(U("WAIT"));
			_log.Log(_log.Time().append(rid).append(U(" Request on has been delayed.")));
		}
		else
		{
			_slocker.lock();
			_acss.push_back(new ACSObj{ *(*room), _usr.admin.deffanspeed });
			_acss.back()->Serve(ctemp);
			_slocker.unlock();

			msg[U("State")] = json::value::string(U("ON"));
			_log.Log(_log.Time().append(rid).append(U(" Request on has been handled.")));
		}
	}
	catch (...)
	{
		msg[U("State")] = json::value::string(U("ERROR"));
		_log.Log(_log.Time().append(L"Room does not exist."));
	}

	if (_usr.admin.defmode == Room::mode_t::HOT)
		msg[U("Mode")] = json::value::string(U("HOT"));
	else if (_usr.admin.defmode == Room::mode_t::COOL)
		msg[U("Mode")] = json::value::string(U("COOL"));
	msg[U("TargetTemp")] = json::value::number(_usr.admin.deftemp);
	msg[U("FeeRate")] = json::value::number(_usr.admin.frate[_usr.admin.deffanspeed]);
	msg[U("Fee")] = json::value::number(0.0);
	msg[U("FanSpeed")] = json::value::number((int64_t)_usr.admin.deffanspeed);

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::REQUESTON, msg });
	(*room)->rponsecnt = 1;
}

void ACSystem::_requestoff(int64_t id)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);

	json::value msg;
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
		(*room)->inservice = false;

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

		msg[U("Fee")] = json::value::number((*room)->GetTotalfee());
		msg[U("Duration")] = json::value::number((int64_t)(*room)->duration);
		(*room)->Reset();
	}
	catch (...)
	{
		msg[U("Fee")] = json::value::number(0.0);
		msg[U("Duration")] = json::value::number(0);
		_log.Log(_log.Time().append(rid).append(U("Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::REQUESTOFF, msg });
	(*room)->rponsecnt++;
}

void ACSystem::_settemp(int64_t id, double_t ttemp)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);
	wchar_t tt[0xF];
	wsprintf(tt, U("%lf"), ttemp);

	json::value msg;
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

		if (ttemp >= _usr.admin.ltemp && ttemp <= _usr.admin.htemp)
		{
			(*room)->SetTargetTemp(ttemp);
			//msg[U("isOk")] = json::value::string(U("True"));
			msg[U("isOK")] = json::value::boolean(true);
		}
		else
		{
			//msg[U("isOk")] = json::value::string(U("False"));
			msg[U("isOK")] = json::value::boolean(false);
		}
	}
	catch (...)
	{
		//msg[U("isOk")] = json::value::string(U("False"));
		msg[U("isOK")] = json::value::boolean(false);
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::SETTEMP, msg });
	(*room)->rponsecnt++;
}

void ACSystem::_setfanspeed(int64_t id, Room::speed_t fanspeed)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);
	wchar_t fs[0xF];
	wsprintf(fs, U("%lf"), fanspeed);

	json::value msg;
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

		msg[U("FeeRate")] = json::value::number(_usr.admin.frate[(*room)->GetFanspeed()]);
	}
	catch (...)
	{
		msg[U("FeeRate")] = json::value::number(0.0);
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::SETFANSPEED, msg });
	(*room)->rponsecnt++;
}

void ACSystem::_fetchfee(int64_t id)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);

	json::value msg;
	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" are fectching the fee.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->rquestcnt++;

		msg[U("FeeRate")] = json::value::number(_usr.admin.frate[(*room)->GetFanspeed()]);
		msg[U("Fee")] = json::value::number((*room)->GetTotalfee());
	}
	catch (...)
	{
		msg[U("FeeRate")] = json::value::number(0.0);
		msg[U("Fee")] = json::value::number(0.0);
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::FETCHFEE, msg });
	(*room)->rponsecnt++;
}

void ACSystem::_notify(int64_t id, double_t ctemp)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), id);
	wchar_t ct[0xF];
	wsprintf(ct, U("%lf"), ctemp);

	json::value msg;
	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" notifies current temperature as ")).append(ct).append(U("¡æ.")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->rquestcnt++;

		(*room)->ctemp = ctemp;

		int64_t state = (int64_t)(*room)->inservice;
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

		if (state == 0)
			msg[U("State")] = json::value::string(U("OFF"));
		else if (state == 1)
			msg[U("State")] = json::value::string(U("ON"));
		else if (state == 2)
			msg[U("State")] = json::value::string(U("WAIT"));
		else if (state == 3)
			msg[U("State")] = json::value::string(U("SLEEP"));
	}
	catch (...)
	{
		msg[U("State")] = json::value::string(U("OFF"));
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::TEMPNOTIFICATION, msg });
	(*room)->rponsecnt++;
}