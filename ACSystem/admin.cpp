#include "pch.h"
#include "system.h"

void ACSystem::_admin()
{
	ACMessage msg;
	Room::mode_t mode;
	while (_onrunning)
	{
		msg.type = ACMsgType::INVALID;
		WaitForSingleObject(_adminspr, 1000);

		_adminlocker.lock();
		if (_admins.size() > 0)
		{
			msg = _admins.front();
			_admins.pop();
		}
		_adminlocker.unlock();

		switch (msg.type)
		{
		case ACMsgType::POWERON:
			_poweron();
			break;
		case ACMsgType::SETPARAM:
			if (msg.body.has_field(U("Mode"))
				&& msg.body.has_field(U("TempHighLimit"))
				&& msg.body.has_field(U("TempLowLimit"))
				&& msg.body.has_field(U("DefaultTargetTemp"))
				&& msg.body.has_field(U("FeeRateH"))
				&& msg.body.has_field(U("FeeRateM"))
				&& msg.body.has_field(U("FeeRateL")))
			{
				if (msg.body.at(U("Mode")).as_string().compare(U("HOT")) == 0)
					mode = Room::mode_t::HOT;
				else if (msg.body.at(U("Mode")).as_string().compare(U("COOL")) == 0)
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
			}
			break;
		case ACMsgType::STARTUP:
			_startup();
			break;
		case ACMsgType::SHUTDOWN:
			_shutdown();
			break;
		case ACMsgType::MONITOR:
			if (msg.body.has_field(U("RoomId")))
				_monitor(msg.body.at(U("RoomId")).as_integer());
			break;
		default:
			break;
		}
	}

}

void ACSystem::_poweron()
{
	_log.Log(_log.Time().append(U("Administrator requests to power on the system.")));

	_dlocker.lock();
	json::value msg;
	int64_t handler = _usr.admin.handler;
	if (_usr.admin.state != Admin::state_t::OFF)
	{
		msg[U("state")] = json::value::string(U("SetMode"));
		_log.Log(_log.Time().append(U("System had been started already.")));
	}
	else
	{
		_usr.admin.state = Admin::state_t::SET;
		_usr.admin.latest = std::time(nullptr);
		_usr.admin.opt = Admin::opt_t::POWERON;
		_usr.admin.start = _usr.admin.latest;

		msg[U("state")] = json::value::string(U("SetMode"));
	}
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::POWERON, msg });
}

void ACSystem::_setparam(Room::mode_t mode, double_t ht, double_t lt, double_t dt, double_t hf, double_t mf, double_t lf)
{
	_log.Log(_log.Time().append(U("Administrator requests to set the system.")));

	_dlocker.lock();
	json::value msg;
	int64_t handler = _usr.admin.handler;
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
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::SETPARAM, msg });
}

void ACSystem::_startup()
{
	_log.Log(_log.Time().append(U("Administrator requests to start the system.")));

	_dlocker.lock();
	json::value msg;
	int64_t handler = _usr.admin.handler;
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

		_onstartup = true;
		_ccontroller = std::move(std::thread{ std::bind(&ACSystem::_check, this) });
		_acontroller = std::move(std::thread{ std::bind(&ACSystem::_alive, this) });

		msg[U("state")] = json::value::string(U("ready"));
		_log.Log(_log.Time().append(U("Start the system correctly.")));
	}
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::STARTUP, msg });
}

void ACSystem::_monitor(int64_t roomid)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), roomid);
	_log.Log(_log.Time().append(U("Administrator needs ")).append(rid).append(U(".")));

	_dlocker.lock();
	json::value msg;
	int64_t handler = _usr.admin.handler;
	if (_usr.admin.state != Admin::state_t::READY)
	{
		msg[U("state")] = json::value::string(U("OFF"));
		msg[U("CurrentTemp")] = json::value::number(0);
		msg[U("TargetTemp")] = json::value::number(0);
		msg[U("Fan")] = json::value::number(0);
		msg[U("FeeRate")] = json::value::number(0.0);
		msg[U("Fee")] = json::value::number(0.0);
		msg[U("Duration")] = json::value::number(0);
	}
	else
	{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [roomid](const Room* cur) {
			return cur->id == roomid;
		});
		if (room != _usr.rooms.end())
		{
			time_t duration = 0;
			auto sobj = std::find_if(_acss.begin(), _acss.end(), [&room](const ACSObj* obj) {
				return (*room)->id == obj->room.id;
			});
			if (sobj != _acss.end())
				duration = std::time(nullptr) - (*sobj)->timestamp;

			Room::state_t state = (*room)->state;
			double_t ctemp = (*room)->ctemp;
			double_t ttemp = (*room)->GetTargetTemp();
			Room::speed_t fspeed = (*room)->GetFanspeed();
			double_t frate = _usr.admin.frate[fspeed];
			double_t fee = (*room)->GetTotalfee();

			switch (state)
			{
			case Room::state_t::SERVICE:
				msg[U("state")] = json::value::string(U("ON"));
				break;
			case Room::state_t::STOPPED:
				msg[U("state")] = json::value::string(U("OFF"));
				break;
			case Room::state_t::SUSPEND:
				msg[U("state")] = json::value::string(U("WAIT"));
				break;
			case Room::state_t::SLEEPED:
				msg[U("state")] = json::value::string(U("SLEEP"));
				break;
			default:
				break;
			}
			msg[U("CurrentTemp")] = json::value::number(ctemp);
			msg[U("TargetTemp")] = json::value::number((int64_t)ttemp);
			msg[U("Fan")] = json::value::number((int64_t)fspeed);
			msg[U("FeeRate")] = json::value::number(frate);
			msg[U("Fee")] = json::value::number(fee);
			msg[U("Duration")] = json::value::number((int64_t)duration);
		}
		else
		{
			msg[U("state")] = json::value::string(U("OFF"));
			msg[U("CurrentTemp")] = json::value::number(0);
			msg[U("TargetTemp")] = json::value::number(0);
			msg[U("Fan")] = json::value::number(0);
			msg[U("FeeRate")] = json::value::number(0.0);
			msg[U("Fee")] = json::value::number(0.0);
			msg[U("Duration")] = json::value::number(0);
		}
	}
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::MONITOR, msg });
}

void ACSystem::_shutdown()
{
	_log.Log(_log.Time().append(U("Administrator requests to shutdown the system.")));

	_dlocker.lock();
	json::value msg;
	int64_t handler = _usr.admin.handler;
	if (_usr.admin.state != Admin::state_t::READY)
	{
		msg[U("isOK")] = json::value::boolean(false);
		_log.Log(_log.Time().append(U("Failed to shutdown the system.")));
		_com.PushMessage(ACMessage{ handler, ACMsgType::SHUTDOWN, msg });
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

			msg[U("isOK")] = json::value::boolean(true);
			_log.Log(_log.Time().append(U("Shutdown the system correctly.")));

			_com.PushMessage(ACMessage{ handler, ACMsgType::SHUTDOWN, msg });

			_onstartup = false;
			if (_ccontroller.joinable())
				_ccontroller.join();

			if (_acontroller.joinable())
				_acontroller.join();

			_acss.clear();
			_acws.clear();

			_log.Log(_log.Time().append(U("System has been shutdown.")));
		}
		else
		{
			msg[U("isOK")] = json::value::boolean(false);
			_log.Log(_log.Time().append(U("Failed to shutdown the system (Some rooms are in service).")));
			_com.PushMessage(ACMessage{ handler, ACMsgType::SHUTDOWN, msg });
		}
	}
	_dlocker.unlock();
}