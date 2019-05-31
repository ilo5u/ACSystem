#include "stdafx.h"
#include "usr.h"

void Invoice::Prepare(opt_t opt, time_t request)
{
	_plocker.lock();
	_prep[opt] = request;
	_plocker.unlock();
}

void Invoice::Store(opt_t opt, time_t response, int64_t fanspeed, double_t feerate, double_t fee)
{
	_plocker.lock();
	auto index = std::find_if(_prep.begin(), _prep.end(), [&opt](const std::pair<opt_t, time_t>& pr) {
		return pr.first == opt;
	});
	if (index != _prep.end())
	{
		// dbms

		_prep.erase(index);
	}
	_plocker.unlock();
}

std::list<Invoice::record_t> Invoice::Load(time_t datein, time_t dateout)
{
	// dbms
	return {};
}

Room::Room(int64_t roomid): 
	id(roomid) 
{ 
}

Room::~Room()
{
}

void Room::On(double_t ct)
{
	if (_charging.joinable())
		_charging.join();

	_charging = std::move(std::thread{ std::bind(&Room::_on, this, ct) });
}

Room::speed_t Room::GetFanspeed()
{
	speed_t fs{ speed_t::NLL };
	if (std::fabs(ctemp - _ttemp) > 0.5)
	{
		_flocker.lock();
		fs = _fanspeed;
		_flocker.unlock();
	}

	return fs;
}

double_t Room::GetTargetTemp()
{
	return _ttemp;
}

double_t Room::GetTotalfee()
{
	_flocker.lock(); 
	double_t tf = _totalfee; 
	_flocker.unlock(); 
	return tf;
}

void Room::SetFanspeed(speed_t fs, double_t fr)
{
	// invoice.dbms()

	_flocker.lock();
	_fanspeed = fs;
	_feerate = fr;
	invoice.Store(Invoice::opt_t::SETFANSPEED, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
	_flocker.unlock();
}

void Room::SetTargetTemp(double_t tt)
{
	_ttemp = tt;

	_flocker.lock();
	invoice.Store(Invoice::opt_t::SETTEMP, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
	_flocker.unlock();
}

void Room::Reset()
{
	state = state_t::STOPPED;

	if (_charging.joinable())
		_charging.join();

	invoice.Store(Invoice::opt_t::REQUESTOFF, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
	// bill.dbms(id, datein, datein + period, totalfee)

	duration = 0;
}

void Room::_on(double_t ct)
{
	invoice.Store(Invoice::opt_t::REQUESTON, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);

	ctemp = ct;
	state = state_t::SERVICE;

	bool off = false;
	while (!off)
	{
		std::this_thread::sleep_for(std::chrono::seconds{ 60 });

		duration++;
		switch (state)
		{
		case Room::state_t::SERVICE:
		case Room::state_t::SUSPEND:
			if (std::fabs(ctemp - _ttemp) > 0.5)
			{
				_flocker.lock();
				_totalfee += _feerate;
				_flocker.unlock();
			}
			break;
		case Room::state_t::STOPPED:
			off = true;
		default:
			break;
		}
	}
}