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
			if (msg.body.has_field(U("RoomId"))
				&& msg.body.has_field(U("CurrentRoomTemp")))
			{
				_requeston(
					msg.body.at(U("RoomId")).as_integer(),
					msg.body.at(U("CurrentRoomTemp")).as_double()
				);
			}
			break;
		case ACMsgType::REQUESTOFF:
			if (msg.body.has_field(U("RoomId")))
				_requestoff(msg.body.at(U("RoomId")).as_integer());
			break;
		case ACMsgType::FETCHFEE:
			if (msg.body.has_field(U("RoomId")))
				_fetchfee(msg.body.at(U("RoomId")).as_integer());
			break;
		case ACMsgType::SETTEMP:
			if (msg.body.has_field(U("RoomId"))
				&& msg.body.has_field(U("TargetTemp")))
			{
				_settemp(
					msg.body.at(U("RoomId")).as_integer(),
					(double_t)msg.body.at(U("TargetTemp")).as_integer()
				);
			}
			break;
		case ACMsgType::SETFANSPEED:
			if (msg.body.has_field(U("RoomId"))
				&& msg.body.has_field(U("FanSpeed")))
			{
				_setfanspeed(
					msg.body.at(U("RoomId")).as_integer(),
					(Room::speed_t)msg.body.at(U("FanSpeed")).as_integer()
				);
			}
			break;
		case ACMsgType::TEMPNOTIFICATION:
			if (msg.body.has_field(U("RoomId"))
				&& msg.body.has_field(U("CurrentRoomTemp")))
			{
				_notify(
					msg.body.at(U("RoomId")).as_integer(),
					msg.body.at(U("CurrentRoomTemp")).as_double()
				);
			}
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
		_log.Log(_log.Time().append(U("Room handling method crashed.")));
	}
}

void ACSystem::_requeston(int64_t id, double_t ctemp)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), id);

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

		(*room)->SetTargetTemp(_usr.admin.deftemp);
		(*room)->SetFanspeed(_usr.admin.deffanspeed, _usr.admin.frate[_usr.admin.deffanspeed]);

		if ((int64_t)_acss.size() > _capacity)
		{
			_dlocker.lock();
			_acws.push_back(new ACWObj{ *(*room), _usr.admin.deffanspeed, 120 });
			_dlocker.unlock();

			msg[U("State")] = json::value::string(U("WAIT"));
			_log.Log(_log.Time().append(rid).append(U(" Request on has been delayed.")));
		}
		else
		{
			_dlocker.lock();
			_acss.push_back(new ACSObj{ *(*room), _usr.admin.deffanspeed });
			_acss.back()->Serve(ctemp);
			_dlocker.unlock();

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
	std::swprintf(rid, U("%I64d"), id);

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

		_dlocker.lock();

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

		_dlocker.unlock();

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

	wchar_t fr[0xF];
	std::swprintf(fr, U("%lf"), msg[U("FeeRate")].as_double());
	wchar_t dr[0xF];
	std::swprintf(dr, U("%I64d"), (int64_t)msg[U("Duration")].as_double());
	_log.Log(_log.Time().append(rid).append(U(" Fee rate is ")).append(fr).append(U(" and duration is ")).append(dr).append(U(" .")));
}

void ACSystem::_settemp(int64_t id, double_t ttemp)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), id);
	wchar_t tt[0xF];
	std::swprintf(tt, U("%.2lf"), ttemp);

	json::value msg;
	std::list<Room*>::iterator room;
	try
	{
		_log.Log(_log.Time().append(rid).append(U(" Set target temperature as ")).append(tt).append(U(" .")));

		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		(*room)->latest = std::time(nullptr);
		(*room)->invoice.Prepare(Invoice::opt_t::SETTEMP, (*room)->latest);
		(*room)->rquestcnt++;

		if (ttemp >= _usr.admin.ltemp && ttemp <= _usr.admin.htemp)
		{
			(*room)->SetTargetTemp(ttemp);
			msg[U("isOK")] = json::value::boolean(true);
		}
		else
		{
			msg[U("isOK")] = json::value::boolean(false);
		}
	}
	catch (...)
	{
		msg[U("isOK")] = json::value::boolean(false);
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::SETTEMP, msg });
	(*room)->rponsecnt++;
}

void ACSystem::_setfanspeed(int64_t id, Room::speed_t fanspeed)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), id);
	wchar_t fs[0xF];
	std::swprintf(fs, U("%I64d"), (int64_t)fanspeed);

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

		_dlocker.lock();

		auto sobj = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
			return (*room)->id == (*obj).room.id;
		});

		if (sobj != _acss.end())
		{
			(*sobj)->fanspeed = fanspeed;
			(*sobj)->room.SetFanspeed(fanspeed, _usr.admin.frate[fanspeed]);
			_log.Log(_log.Time().append(rid).append(U(" still stays in Service-Queue.")));
		}
		else if ((int64_t)_acss.size() < _capacity)
		{
			auto wobj = std::find_if(_acws.begin(), _acws.end(), [&room](const ACWObj* obj) {
				return (*room)->id == (*obj).room.id;
			});
			if (wobj != _acws.end())
			{
				delete *wobj;
				*wobj = nullptr;
				_acws.erase(wobj);
				_log.Log(_log.Time().append(rid).append(U(" has been removed from Wait-Queue.")));
			}
			_acss.push_back(new ACSObj{ *(*room), fanspeed });
			(*room)->SetFanspeed(fanspeed, _usr.admin.frate[fanspeed]);
			_log.Log(_log.Time().append(rid).append(U(" has been movee into Service-Queue.")));
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
					if ((*elem)->fanspeed <= fanspeed && (*elem)->fanspeed <= minfs)
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
						_log.Log(_log.Time().append(rid).append(U(" still stays in Wait-Queue cause its fanspeed is lower than all.")));
					}
					else
					{
						_acws.push_back(new ACWObj{ *(*room), fanspeed, 120 });
						_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue cause its fanspeed is lower than all.")));
					}
				}
				else
				{
					std::list<std::list<ACSObj*>::iterator> maximal;
					time_t longest = -1;
					for (auto elem = prep.begin(); elem != prep.end(); ++elem)
					{
						time_t dr = (*(*elem))->duration;
						if ((*(*elem))->fanspeed == minfs
							&& dr > longest)
						{
							maximal.push_back(*elem);
							longest = dr;
						}
					}

					if (maximal.size() > 0)
					{
						auto towait = maximal.back();
						_acws.push_back(new ACWObj{ (*towait)->room, (*room)->GetFanspeed(true), 120 });

						std::swprintf(rid, U("%I64d"), (*towait)->room.id);
						_log.Log(_log.Time().append(rid).append(U(" has been  moved into Wait-Queue.")));

						delete *towait;
						*towait = nullptr;
						_acss.erase(towait);

						if (wobj != _acws.end())
						{
							delete *wobj;
							*wobj = nullptr;
							_acws.erase(wobj);
						}

						_acss.push_back(new ACSObj{ *(*room), fanspeed });
						(*room)->SetFanspeed(fanspeed, _usr.admin.frate[fanspeed]);

						_log.Log(_log.Time().append(rid).append(U(" has been  moved into Serice-Queue.")));
					}
				}
			}
			else if (fanspeed == fs)
			{
				if (wobj != _acws.end())
				{
					(*wobj)->tfanspeed = fanspeed;
					(*wobj)->duration = 120;
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
				if (wobj != _acws.end())
				{
					(*wobj)->tfanspeed = fanspeed;
					(*wobj)->duration = INT64_MAX;
					_log.Log(_log.Time().append(rid).append(U(" still stays in Wait-Queue until Service-Queue is available.")));
				}
				else
				{
					_acws.push_back(new ACWObj{ *(*room), fanspeed, INT64_MAX });
					_log.Log(_log.Time().append(rid).append(U(" has been moved into Wait-Queue until Service-Queue is available.")));
				}
			}
		}

		_dlocker.unlock();

		msg[U("FeeRate")] = json::value::number(_usr.admin.frate[fanspeed]);
	}
	catch (...)
	{
		msg[U("FeeRate")] = json::value::number(_usr.admin.frate[fanspeed]);
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::SETFANSPEED, msg });
	(*room)->rponsecnt++;

	wchar_t fr[0xF];
	std::swprintf(fr, U("%lf"), msg[U("FeeRate")].as_double());
	_log.Log(_log.Time().append(rid).append(U(" Fee rate is ")).append(fr).append(U(" .")));
}

void ACSystem::_fetchfee(int64_t id)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), id);

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

	wchar_t fr[0xF];
	std::swprintf(fr, U("%lf"), msg[U("FeeRate")].as_double());
	wchar_t fee[0xF];
	std::swprintf(fee, U("%lf"), msg[U("Fee")].as_double());
	_log.Log(_log.Time().append(rid).append(U(" Fee rate is ")).append(fr).append(U(" and fee is ")).append(fee).append(U(" .")));
}

void ACSystem::_notify(int64_t id, double_t ctemp)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), id);
	wchar_t ct[0xF];
	std::swprintf(ct, U("%.2lf"), ctemp);
	wchar_t tt[0xF];

	json::value msg;
	std::list<Room*>::iterator room;
	try
	{
		room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [id](const Room* cur) {
			return cur->id == id;
		});

		std::swprintf(tt, U("%.2lf"), (*room)->GetTargetTemp());
		_log.Log(_log.Time().append(rid).append(U(" notifies current temperature as ")).append(ct).append(U("¡æ, and target is ")).append(tt).append(U("¡æ.")));

		(*room)->latest = std::time(nullptr);
		(*room)->rquestcnt++;
		(*room)->ctemp = ctemp;
		double_t ttemp = (*room)->GetTargetTemp();

		_dlocker.lock();

		auto ins = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
			return (*room)->id == obj->room.id;
		});
		if (ins != _acss.end())
		{
			if (std::fabs(ctemp - ttemp) < 0.5)
			{
				_acws.push_back(new ACWObj{ (*ins)->room, (*ins)->room.GetFanspeed(true), INT64_MAX });
				(*room)->state = Room::state_t::SLEEPED;

				delete *ins;
				*ins = nullptr;
				_acss.erase(ins);

				if (_acws.size() > 0)
				{
					std::list<std::list<ACWObj*>::iterator> minimal;
					time_t mindr = INT64_MAX;
					for (auto elem = _acws.begin(); elem != _acws.end(); ++elem)
					{
						if ((*elem)->duration <= mindr)
						{
							mindr = (*elem)->duration;
							minimal.push_back(elem);
						}
					}

					auto toserivce = minimal.back();

					_acss.push_back(new ACSObj{ (*toserivce)->room, (*toserivce)->tfanspeed });
					(*toserivce)->room.SetFanspeed((*toserivce)->tfanspeed, _usr.admin.frate[(*toserivce)->tfanspeed]);

					std::swprintf(rid, U("%I64d"), (*toserivce)->room.id);
					_log.Log(_log.Time().append(rid).append(U(" has been moved into Service-Queue.")));

					delete *toserivce;
					*toserivce = nullptr;
					_acws.erase(toserivce);
				}
			}
		}
		else
		{
			auto inw = std::find_if(_acws.begin(), _acws.end(), [&room](const ACWObj* obj) {
				return (*room)->id == obj->room.id;
			});
			if (inw != _acws.end())
			{
				if (std::fabs(ctemp - ttemp) < 0.5)
				{
					(*room)->state = Room::state_t::SLEEPED;
				}
				else
				{
					if ((int64_t)_acss.size() < _capacity)
					{
						_acss.push_back(new ACSObj{ (*inw)->room, (*inw)->tfanspeed });

						std::swprintf(rid, U("%I64d"), (*inw)->room.id);
						_log.Log(_log.Time().append(rid).append(U(" has been moved into Service-Queue.")));

						delete *inw;
						*inw = nullptr;
						_acws.erase(inw);
					}
					else
					{
						(*room)->state = Room::state_t::SUSPEND;
					}
				}
			}
		}

		_dlocker.unlock();

		switch ((*room)->state)
		{
		case Room::state_t::SERVICE:
			msg[U("State")] = json::value::string(U("ON"));
			break;
		case Room::state_t::STOPPED:
			msg[U("State")] = json::value::string(U("OFF"));
			break;
		case Room::state_t::SUSPEND:
			msg[U("State")] = json::value::string(U("WAIT"));
			break;
		case Room::state_t::SLEEPED:
			msg[U("State")] = json::value::string(U("SLEEP"));
			break;
		default:
			break;
		}
	}
	catch (...)
	{
		msg[U("State")] = json::value::string(U("OFF"));
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ (*room)->handler, ACMsgType::TEMPNOTIFICATION, msg });
	(*room)->rponsecnt++;

	_log.Log(_log.Time().append(rid).append(U(" is ")).append(msg[U("State")].as_string()));
}