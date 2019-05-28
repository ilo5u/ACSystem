#pragma once

struct UBase
{
	ACCom::Handler handler{0};

	time_t latest{0};
	time_t duration{0};
	int64_t rquestcnt{0};
	int64_t rponsecnt{0};
	bool inservice{false};
};

struct Bill
{

};

struct Invoice
{
	enum class opt_t
	{
		POWERON,
		POWEROFF,
		SETTEMP,
		SETFANSPEED
	};

	struct record_t
	{
		time_t timestamp;
		opt_t opt;
	};
	std::list<record_t> records;
};

class Room :
	public UBase
{
public:
	enum class state_t
	{
		INVALID,
		SERVICE,
		STOPPED,
		SUSPEND
	};

	enum class opt_t
	{
		IDLE,
		REQUESTON,
		REQUESTOFF,
		SETTEMP,
		SETFANSPEED,
		TEMPNOTIFICATION
	};

	enum class mode_t
	{
		HOT,
		COOL
	};

public:
	Room(int64_t roomid) : id(roomid) { }
	~Room()
	{
	}

	Room(const Room&) = delete;
	Room(Room&&) = delete;
	Room& operator=(const Room&) = delete;
	Room& operator=(Room&&) = delete;

public:
	int64_t id;
	mode_t mode;
	double_t ctemp{ 0 };
	double_t ttemp{ 0 };

	state_t state{ state_t::STOPPED };
	opt_t opt{ opt_t::IDLE };
	Bill bill;
	Invoice invoice;

private:
	double_t _fanspeed{0.0};
	double_t _feerate{0.0};
	double_t _totalfee{0.0};

	std::thread _charging;
	std::mutex _slocker;
	std::mutex _flocker;
public:
	void On(double_t ct)
	{
		if (_charging.joinable())
			_charging.join();

		_charging = std::move(std::thread{ std::bind(&Room::_on, this), ct });
	}

public:
	double_t GetFanspeed() { _flocker.lock(); double_t fs = _fanspeed; _flocker.unlock(); return fs; }
	double_t GetFeerate() { _flocker.lock(); double_t fr = _feerate; _flocker.unlock(); return fr; }
	double_t GetTotalfee() { _flocker.lock(); double_t tf = _totalfee; _flocker.unlock(); return tf; }

public:
	void SetFanspeed(double_t fs) { _flocker.lock(); _fanspeed = fs; _flocker.unlock(); }
	void SetFeerate(double_t fr) { _flocker.lock(); _feerate = fr; _flocker.unlock(); }

private:
	void _on(double_t ct)
	{
		ctemp = ct;
		state = state_t::SERVICE;
		opt = opt_t::REQUESTON;

		bool off = false;
		while (!off)
		{
			std::this_thread::sleep_for(std::chrono::seconds{ 60 });

			state_t cs = state_t::INVALID;
			_slocker.lock();
			cs = state;
			_slocker.unlock();

			switch (cs)
			{
			case Room::state_t::SERVICE:
			case Room::state_t::SUSPEND:
				_flocker.lock();
				_totalfee += _feerate;
				_flocker.unlock();
				break;
			case Room::state_t::STOPPED:
				off = true;
			default:
				break;
			}
		}
	}
};

struct Admin :
	public UBase
{
	enum class state_t
	{
		OFF,
		SET,
		READY
	};

	enum class opt_t
	{
		IDLE,
		POWERON,
		POWEROFF,
		MONITOR
	};

	Room::mode_t defmode;
	double_t deftemp{0.0};
	double_t deffanspeed{0.0};
	double_t frlow{0.0};
	double_t frmid{0.0};
	double_t frhgh{0.0};

	state_t state{state_t::OFF};
	opt_t opt{opt_t::IDLE};

	time_t start{0};
	time_t duration{0};
};

struct Rpt :
	public UBase
{
	enum class opt_t
	{
		IDLE,
		FETCHBILL,
		FETCHINVOICE
	};
};

struct Mgr :
	public UBase
{
	enum class opt_t
	{
		IDLE,
		FETCHREPORT
	};
};

struct ACUsr
{
	std::list<Room> rooms;
	Admin admin;
	Rpt rpt;
	Mgr mgr;
};

class ACSObj
{
public:
	ACSObj(Room& r, double_t fs) :
		room(r), fanspeed(fs), timestamp(std::time(nullptr)),
		_running(true), _sid(std::bind(&ACSObj::_service, this)), _dlocker(),
		_duration(0)
	{
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
	double_t fanspeed;
	time_t timestamp;

private:
	bool _running;
	std::thread _sid;
	std::mutex _dlocker;
	time_t _duration;

public:
	void Serve(double_t ctemp) { room.On(ctemp); }
	time_t GetDuration() {}

private:
	void _service()
	{
		while (_running)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));

			_dlocker.lock();
			_duration++;
			_dlocker.unlock();
		}
	}
};

//class ACServiceQueue
//{
//public:
//	ACServiceQueue();
//	~ACServiceQueue();
//
//	ACServiceQueue(const ACServiceQueue&) = delete;
//	ACServiceQueue(ACServiceQueue&&) = delete;
//	ACServiceQueue& operator=(const ACServiceQueue&) = delete;
//	ACServiceQueue& operator=(ACServiceQueue&&) = delete;
//private:
//	std::list<ACSObj> _objs;
//};

class ACWObj
{
public:
	ACWObj(Room& r, double_t tf, time_t delay) :
		room(r), tfanspeed(tf),
		timestamp(std::time(nullptr)), duration(delay)
	{
	}

	~ACWObj() = default;

	ACWObj(const ACWObj&) = delete;
	ACWObj(ACWObj&&) = delete;
	ACWObj& operator=(const ACWObj&) = delete;
	ACWObj& operator=(ACWObj&&) = delete;

public:
	Room& room;
	double_t tfanspeed;
	time_t timestamp;
	time_t duration;
};

//class ACWaitQueue
//{
//public:
//	ACWaitQueue();
//	~ACWaitQueue();
//
//	ACWaitQueue(const ACWaitQueue&) = delete;
//	ACWaitQueue(ACWaitQueue&&) = delete;
//	ACWaitQueue& operator=(const ACWaitQueue&) = delete;
//	ACWaitQueue& operator=(ACWaitQueue&&) = delete;
//private:
//	std::list<ACWObj> _objs;
//};

//class ACDispatcher
//{
//public:
//	ACDispatcher();
//	~ACDispatcher();
//
//	ACDispatcher(const ACDispatcher&) = delete;
//	ACDispatcher(ACDispatcher&&) = delete;
//	ACDispatcher& operator=(const ACDispatcher&) = delete;
//	ACDispatcher& operator=(ACDispatcher&&) = delete;
//private:
//	std::list<ACSObj> _acss;
//	std::list<ACWObj> _acws;
//};

class ACSystem
{
public:
	ACSystem(ACCom& com, ACLog& log, const std::initializer_list<int64_t>& roomids);
	~ACSystem();

	ACSystem(const ACSystem&) = delete;
	ACSystem(ACSystem&&) = delete;
	ACSystem& operator=(const ACSystem&) = delete;
	ACSystem& operator=(ACSystem&&) = delete;

private:
	ACCom& _com;
	ACLog& _log;
	ACUsr  _usr;

	const int64_t _capacity;
	std::mutex _slocker;
	std::list<ACSObj*> _acss;

	std::mutex _wlocker;
	std::list<ACWObj*> _acws;

	std::thread _mcontroller;
	void _master();

	bool _poweron;
	std::thread _ccontroller;
	void _check();
private:
	void _room(const ACMessage& msg);
	void _requeston(int64_t id, double_t ctemp);
	void _requestoff(int64_t id);
	void _settemp(int64_t id, double_t ttemp);
	void _setfanspeed(int64_t id, double_t fanspeed);
	void _notification(int64_t id, double_t ctemp);

	void _admin(const ACMessage& msg);
	void _mgr(const ACMessage& msg);
	void _rpt(const ACMessage& msg);
};