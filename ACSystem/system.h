#pragma once

class ACSObj
{
public:
	ACSObj(Room& r, Room::speed_t fs) :
		room(r), fanspeed(fs), timestamp(std::time(nullptr)), duration(0),
		_running(true), _sid()
	{
		r.state = Room::state_t::SERVICE;
		r.dptcount++;

		_sid = std::move(std::thread{ std::bind(&ACSObj::_service, this) });
	}

	~ACSObj()
	{
		_running = false;

		if (_sid.joinable())
			_sid.join();
	}

	ACSObj(const ACSObj&) = delete;
	ACSObj(ACSObj&&) = delete;
	ACSObj& operator=(const ACSObj&) = delete;
	ACSObj& operator=(ACSObj&&) = delete;

public:
	Room& room;
	Room::speed_t fanspeed;
	time_t timestamp;
	std::atomic<time_t> duration;

private:
	std::atomic<bool> _running;
	std::thread _sid;

public:
	void Serve(double_t ctemp) { room.On(ctemp); }

private:
	void _service()
	{
		while (_running)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			duration++;
		}
	}
};

class ACWObj
{
public:
	ACWObj(Room& r, Room::speed_t tf, time_t delay) :
		room(r), tfanspeed(tf),
		timestamp(std::time(nullptr)), duration(delay)
	{
		r.state = Room::state_t::SUSPEND;
		r.dptcount++;
	}

	~ACWObj() = default;

	ACWObj(const ACWObj&) = delete;
	ACWObj(ACWObj&&) = delete;
	ACWObj& operator=(const ACWObj&) = delete;
	ACWObj& operator=(ACWObj&&) = delete;

public:
	Room& room;
	Room::speed_t tfanspeed;
	time_t timestamp;
	time_t duration;
};

class ACSystem
{
public:
	ACSystem(ACCom& com, ACLog& log, ACDbms& dbms, const std::vector<int64_t>& roomids);
	~ACSystem();

	ACSystem(const ACSystem&) = delete;
	ACSystem(ACSystem&&) = delete;
	ACSystem& operator=(const ACSystem&) = delete;
	ACSystem& operator=(ACSystem&&) = delete;

public:
	void Wait();

private:
	ACCom& _com;
	ACLog& _log;
	ACUsr  _usr;

	const int64_t _capacity;

	std::mutex _dlocker;
	std::list<ACSObj*> _acss;
	std::list<ACWObj*> _acws;
	std::map<int64_t, bool> _watcher;

	std::atomic<bool> _onrunning;
	std::thread _mcontroller;
	void _master();

	std::atomic<bool> _onstartup;
	std::thread _ccontroller;
	void _check();

	std::thread _acontroller;
	void _alive();
private:
	void _room(const ACMessage& msg);
	void _requeston(int64_t id, double_t ctemp);
	void _requestoff(int64_t id);
	void _settemp(int64_t id, double_t ttemp);
	void _setfanspeed(int64_t id, Room::speed_t fanspeed);
	void _fetchfee(int64_t id);
	void _notify(int64_t id, double_t ctemp);

	void _admin(const ACMessage& msg);
	void _poweron();
	void _setparam(Room::mode_t mode,
		double_t ht, double_t lt, double_t dt, 
		double_t hf, double_t mf, double_t lf
	);
	void _startup();
	void _monitor(int64_t roomid);
	void _shutdown();

	void _mgr(const ACMessage& msg);
	void _fetchreport(int64_t roomid, Mgr::rtype_t rt, time_t head);

	void _rpt(const ACMessage& msg);
	void _fetchbill(int64_t roomid, time_t din, time_t dout);
	void _fetchinvoice(int64_t roomid, time_t din, time_t dout);
};