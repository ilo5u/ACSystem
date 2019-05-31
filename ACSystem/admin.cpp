#include "pch.h"
#include "system.h"

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
			if (msg.body.at(U("Mode")).as_string().compare(U("HOT")) == 0)
				mode = Room::mode_t::HOT;
			else if (msg.body.at(U("Mode")).as_string().compare(U("COOL")) == 0)
				mode = Room::mode_t::COOL;
			_setparam(
				mode,
				msg.body.at(U("TempHighLimit")).as_double(),
				msg.body.at(U("TempLowLimit")).as_double(),
				msg.body.at(U("DefaultTargetTemp")).as_double(),
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
		_log.Log(_log.Time().append(U("Admin handling method crashed.")));
	}
}

void ACSystem::_poweron()
{
	_usr.admin.rquestcnt = 1;

	_log.Log(_log.Time().append(U("Administrator requests to power on the system.")));

	json::value msg;
	if (_usr.admin.state != Admin::state_t::OFF)
	{
		msg[U("state")] = json::value::string(U("error"));
		_log.Log(_log.Time().append(U(" System had been started already.")));
	}
	else
	{
		_usr.admin.state = Admin::state_t::SET;
		_usr.admin.latest = std::time(nullptr);
		_usr.admin.opt = Admin::opt_t::POWERON;
		_usr.admin.start = _usr.admin.latest;

		msg[U("state")] = json::value::string(U("SetMode"));
	}

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
		msg[U("isOK")] = json::value::boolean(false);
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

		msg[U("isOK")] = json::value::boolean(true);
		_log.Log(_log.Time().append(U("Set params correctly.")));
	}

	_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::SETPARAM, msg });
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
		_log.Log(_log.Time().append(U("Failed to start the system.")));
	}
	else
	{
		_usr.admin.latest = std::time(nullptr);
		_usr.admin.opt = Admin::opt_t::STARTUP;
		_usr.admin.state = Admin::state_t::READY;

		_ccontroller = std::move(std::thread{ std::bind(&ACSystem::_check, this) });

		msg[U("state")] = json::value::string(U("ready"));
		_log.Log(_log.Time().append(U("Start the system correctly.")));
	}

	_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::STARTUP, msg });
	_usr.admin.rponsecnt++;
}

void ACSystem::_monitor(int64_t roomid)
{
	_usr.admin.rquestcnt++;

	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), roomid);

	json::value msg;
	std::list<Room*>::iterator room = _usr.rooms.end();
	try
	{
		if (_usr.admin.state != Admin::state_t::READY)
			throw std::exception{ "System state error." };
	
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
		Room::state_t state = (*room)->state;
		double_t ctemp = (*room)->ctemp;
		double_t ttemp = (*room)->GetTargetTemp();
		Room::speed_t fspeed = (*room)->GetFanspeed();
		double_t frate = _usr.admin.frate[fspeed];
		double_t fee = (*room)->GetTotalfee();

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
		msg[U("CurrentTemp")] = json::value::number(ctemp);
		msg[U("TargetTemp")] = json::value::number(ttemp);
		msg[U("Fan")] = json::value::number((int64_t)fspeed);
		msg[U("FeeRate")] = json::value::number(frate);
		msg[U("Fee")] = json::value::number(fee);
		msg[U("Duration")] = json::value::number((int64_t)duration);
	}
	catch (std::exception&)
	{
		msg[U("state")] = json::value::string(U("error"));
		_log.Log(_log.Time().append(rid).append(U(" System state error.")));
	}
	catch (...)
	{
		msg[U("state")] = json::value::string(U("error"));
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::MONITOR, msg });
	_usr.admin.rponsecnt++;
}

void ACSystem::_shutdown()
{
	_usr.admin.rquestcnt++;

	_log.Log(_log.Time().append(U("Administrator requests to shutdown the system.")));
	json::value msg;
	if (_usr.admin.state != Admin::state_t::READY)
	{
		msg[U("isOK")] = json::value::boolean(false);
		_log.Log(_log.Time().append(U("Failed to shutdown the system.")));
	}
	else
	{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [](const Room* cur) {
			return (*cur).inservice == true;
		});
		if (room == _usr.rooms.end())
		{
			_usr.admin.latest = std::time(nullptr);
			_usr.admin.opt = Admin::opt_t::SHUTDOWN;
			_usr.admin.state = Admin::state_t::SET;

			_onstartup = false;

			msg[U("isOK")] = json::value::boolean(true);
			_log.Log(_log.Time().append(U("Shutdown the system correctly.")));

			if (_ccontroller.joinable())
				_ccontroller.join();

			_acss.clear();
			_acws.clear();
		}
		else
		{
			msg[U("isOK")] = json::value::boolean(false);
			_log.Log(_log.Time().append(U("Failed to shutdown the system (Some rooms are in service).")));
		}
	}

	_com.PushMessage(ACMessage{ _usr.admin.handler, ACMsgType::SHUTDOWN, msg });
	_usr.admin.rponsecnt++;
}