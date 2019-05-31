#include "stdafx.h"
#include "usr.h"

Bill::Bill(Room* room, ACDbms& dbms) :
	_room(room), _dbms(dbms)
{
}

json::value Bill::Load(time_t datein, time_t dateout)
{
	json::array data = _dbms.Select(ACDbms::obj_t::BILL, _room->id, datein, dateout).as_array();

	try
	{
		double_t fee = 0.0;
		for (json::array::iterator elem = data.begin(); elem != data.end(); ++elem)
			fee += (*elem)[U("totalfee")].as_double();

		json::value bill;
		bill[U("RoomId")] = json::value::number(_room->id);
		bill[U("TotalFee")] = json::value::number(fee);
		bill[U("DateIn")] = json::value::number(datein);
		bill[U("DateOut")] = json::value::number(dateout);
		return bill;
	}
	catch (...)
	{
		return {};
	}
}

Report::Report(Room* room, ACDbms& dbms) :
	_room(room), _dbms(dbms)
{
}

void Report::Store(int64_t onoff, time_t ontime, time_t offtime, double_t totalfee, int64_t dptcount, int64_t rdrcount, int64_t stpcount, int64_t sfscount)
{
	try
	{
		json::value data;
		data[U("roomid")] = json::value::number(_room->id);
		data[U("onoff")] = json::value::number(onoff);
		data[U("ontime")] = json::value::number(ontime);
		data[U("offtime")] = json::value::number(offtime);
		data[U("totalfee")] = json::value::number(totalfee);
		data[U("dptcount")] = json::value::number(dptcount);
		data[U("rdrcount")] = json::value::number(rdrcount);
		data[U("stpcount")] = json::value::number(stpcount);
		data[U("sfscount")] = json::value::number(sfscount);
		_dbms.Insert(ACDbms::obj_t::REPORT, data);
	}
	catch (...)
	{
	}
}

json::value Report::Load(time_t datein, time_t dateout)
{
	json::array data = _dbms.Select(ACDbms::obj_t::REPORT, _room->id, datein, dateout).as_array();
	try
	{
		int64_t onoff = 0;
		int64_t duration = 0;
		double_t totalfee = 0;
		int64_t dptcount = 0;
		int64_t rdrcount = 0;
		int64_t stpcount = 0;
		int64_t sfscount = 0;
		for (json::array::iterator elem = data.begin(); elem != data.end(); ++elem)
		{
			onoff += (*elem).at(U("TimesOfOnOff")).as_integer();
			duration += (*elem).at(U("Duration")).as_integer();
			totalfee += (*elem).at(U("TotalFee")).as_double();
			dptcount += (*elem).at(U("TimesOfDisptch")).as_integer();
			rdrcount += (*elem).at(U("TimesOfRDR")).as_integer();
			stpcount += (*elem).at(U("TimesOfChangeTemp")).as_integer();
			sfscount += (*elem).at(U("TimesOfChangeFanSpeed")).as_integer();
		}

		json::value report;
		report[U("RoomId")] = json::value::number(_room->id);
		report[U("TimesOfOnOff")] = json::value::number(onoff);
		report[U("Duration")] = json::value::number(duration);
		report[U("TotalFee")] = json::value::number(totalfee);
		report[U("TimesOfDispatch")] = json::value::number(dptcount);
		report[U("NumberOfRDR")] = json::value::number(rdrcount);
		report[U("TimesOfChangeTemp")] = json::value::number(stpcount);
		report[U("TimesOfChangeFanSpeed")] = json::value::number(sfscount);
		return report;
	}
	catch (...)
	{
		return {};
	}
}

Invoice::Invoice(Room* room, ACDbms& dbms) :
	_room(room), _dbms(dbms)
{
}

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
		json::value data;
		data[U("roomid")] = json::value::number(_room->id);
		data[U("rqtime")] = json::value::number(index->second);
		data[U("duration")] = json::value::number(response - index->second);
		data[U("fanspeed")] = json::value::number(fanspeed);
		data[U("feerate")] = json::value::number(feerate);
		data[U("totalfee")] = json::value::number(fee);
		_dbms.Insert(ACDbms::obj_t::INVOICE, data);

		_prep.erase(index);
	}
	_plocker.unlock();
}

json::value Invoice::Load(time_t datein, time_t dateout)
{
	return _dbms.Select(ACDbms::obj_t::INVOICE, _room->id, datein, dateout);
}

Room::Room(int64_t roomid, ACDbms& dbms) :
	id(roomid),
	bill(this, dbms), invoice(this, dbms), report(this, dbms)
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
	_flocker.lock();
	_fanspeed = fs;
	_feerate = fr;
	invoice.Store(Invoice::opt_t::SETFANSPEED, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
	_flocker.unlock();

	sfscount++;
}

void Room::SetTargetTemp(double_t tt)
{
	_ttemp = tt;

	_flocker.lock();
	invoice.Store(Invoice::opt_t::SETTEMP, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
	_flocker.unlock();

	stpcount++;
}

void Room::Reset()
{
	state = state_t::STOPPED;

	if (_charging.joinable())
		_charging.join();

	invoice.Store(Invoice::opt_t::REQUESTOFF, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
	report.Store(2, ontime, ontime + duration, _totalfee, dptcount, rdrcount, stpcount, sfscount);

	duration = 0;
	_totalfee = 0.0;
	dptcount = 0;
	rdrcount = 0;
	stpcount = 0;
	sfscount = 0;
}

void Room::_on(double_t ct)
{
	ontime = std::time(nullptr);
	duration = 0;
	_totalfee = 0.0;
	dptcount = 0;
	rdrcount = 0;
	stpcount = 0;
	sfscount = 0;

	invoice.Store(Invoice::opt_t::REQUESTON, ontime, (int64_t)_fanspeed, _feerate, _totalfee);

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
