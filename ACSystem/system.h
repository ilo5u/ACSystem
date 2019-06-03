#pragma once

class ACSObj
{
public:
	ACSObj(Room& r, Room::speed_t fs) :
		room(r), fanspeed(fs), timestamp(std::time(nullptr))
	{
		room.state = Room::state_t::SERVICE;
		room.dptcount++;
	}

	~ACSObj()
	{
	}

	ACSObj(const ACSObj&) = delete;
	ACSObj(ACSObj&&) = delete;
	ACSObj& operator=(const ACSObj&) = delete;
	ACSObj& operator=(ACSObj&&) = delete;

public:
	Room& room;
	Room::speed_t fanspeed;
	time_t timestamp;

public:
	void Serve(double_t ctemp) { room.On(ctemp); }
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

public:
	void Serve(double_t ctemp) { room.On(ctemp); }
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
	void Start();

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
	void _master();

	std::atomic<bool> _onstartup;
	std::thread _ccontroller;
	void _check();

	std::thread _acontroller;
	void _alive();

	std::vector<std::thread> _ucontroller;
	Semophare _roomspr;
	Semophare _mgrspr;
	Semophare _rptspr;
	Semophare _adminspr;
	std::mutex _roomlocker;
	std::mutex _mgrlocker;
	std::mutex _rptlocker;
	std::mutex _adminlocker;
	std::queue<ACMessage> _rooms;
	std::queue<ACMessage> _mgrs;
	std::queue<ACMessage> _rpts;
	std::queue<ACMessage> _admins;

private:
	void _postroom(const ACMessage& msg);
	void _postmgr(const ACMessage& msg);
	void _postrpt(const ACMessage& msg);
	void _postadmin(const ACMessage& msg);

	void _room();
	void _requeston(int64_t id, double_t ctemp);
	void _requestoff(int64_t id);
	void _settemp(int64_t id, double_t ttemp);
	void _setfanspeed(int64_t id, Room::speed_t fanspeed);
	void _fetchfee(int64_t id);
	void _notify(int64_t id, double_t ctemp);

	void _admin();
	void _poweron();
	void _setparam(Room::mode_t mode,
		double_t ht, double_t lt, double_t dt, 
		double_t hf, double_t mf, double_t lf
	);
	void _startup();
	void _monitor(int64_t roomid);
	void _shutdown();

	void _mgr();
	void _fetchreport(int64_t roomid, Mgr::rtype_t rt, time_t head);

	void _rpt();
	void _fetchbill(int64_t roomid, time_t din, time_t dout);
	void _fetchinvoice(int64_t roomid, time_t din, time_t dout);
};