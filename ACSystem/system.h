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

class Invoice
{
public:
	enum class opt_t
	{
		IDLE,
		REQUESTON,
		REQUESTOFF,
		SETTEMP,
		SETFANSPEED
	};

	struct record_t
	{
		time_t request;
		time_t duration;
		double_t fanspeed;
		double_t feerate;
		double_t fee;
	};

public:
	void Prepare(opt_t opt, time_t request)
	{
		_plocker.lock();
		_prep[opt] = request;
		_plocker.unlock();
	}

	void Store(opt_t opt, time_t response, int64_t fanspeed, double_t feerate, double_t fee)
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

	std::list<record_t> Load(time_t datein, time_t dateout)
	{
		// dbms
	}

private:
	std::mutex _plocker;
	std::map<opt_t, time_t> _prep;
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
		FETCHFEE,
		TEMPNOTIFICATION
	};

	enum class mode_t
	{
		HOT,
		COOL
	};

	enum class speed_t
	{
		NLL,
		LOW,
		MID,
		HGH,
		LMT
	};

public:
	Room(int64_t roomid) : id(roomid) { }
	~Room()
	{
	}

	//Room(const Room& right);
	//Room(Room&&) = delete;
	//Room& operator=(const Room&) = delete;
	//Room& operator=(Room&&) = delete;

public:
	int64_t id;
	mode_t mode;

	opt_t opt{ opt_t::IDLE };
	Bill bill;
	Invoice invoice;

private:
	state_t _state{ state_t::STOPPED };

	double_t _ctemp{ 0 };
	double_t _ttemp{ 0 };

	speed_t _fanspeed;
	double_t _feerate;
	double_t _totalfee{0.0};

	time_t _datein{ 0 };
	time_t _period{ 0 };

	std::thread _charging;
	std::mutex _slocker;
	std::mutex _flocker;
	std::mutex _tlocker;
public:
	void On(double_t ct)
	{
		if (_charging.joinable())
			_charging.join();

		_charging = std::move(std::thread{ std::bind(&Room::_on, this, ct) });
	}

public:
	double_t GetCurrentTemp() { _tlocker.lock(); double_t ct = _ctemp; _tlocker.unlock(); return ct; }
	double_t GetTargetTemp() { _tlocker.lock(); double_t tt = _ttemp; _tlocker.unlock(); return tt; }

	speed_t GetFanspeed()
	{
		speed_t fs{ speed_t::NLL };
		_tlocker.lock();
		if (std::fabs(_ctemp - _ttemp) > 0.5)
		{
			_flocker.lock();
			fs = _fanspeed;
			_flocker.unlock();
		}
		_tlocker.unlock();

		return fs; 
	}

	double_t GetTotalfee() { _flocker.lock(); double_t tf = _totalfee; _flocker.unlock(); return tf; }
	time_t GetPeriod() { _slocker.lock(); time_t pd = _period; _slocker.unlock(); return pd; }

	state_t GetState()
	{
		_slocker.lock();
		state_t state = _state;
		_slocker.unlock();
		return state;
	}

public:
	void SetFanspeed(speed_t fs, double_t fr)
	{
		// invoice.dbms()

		_flocker.lock(); 
		_fanspeed = fs;
		_feerate = fr;
		invoice.Store(Invoice::opt_t::SETFANSPEED, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
		_flocker.unlock();
	}

	void SetCurrentTemp(double_t ct)
	{
		_tlocker.lock();
		_ctemp = ct;
		_tlocker.unlock();
	}

	void SetTargetTemp(double_t tt)
	{
		_tlocker.lock();
		_ttemp = tt;
		_tlocker.unlock();

		_flocker.lock();
		invoice.Store(Invoice::opt_t::SETTEMP, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
		_flocker.unlock();
	}

	void SetState(Room::state_t state)
	{
		_slocker.lock();
		_state = state;
		_slocker.unlock();
	}

	void Reset() 
	{
		_slocker.lock(); 
		_state = state_t::STOPPED; 
		_slocker.unlock(); 

		if (_charging.joinable())
			_charging.join();

		invoice.Store(Invoice::opt_t::REQUESTOFF, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);
		// bill.dbms(id, datein, datein + period, totalfee)

		_period = 0;
	}

private:
	void _on(double_t ct)
	{
		invoice.Store(Invoice::opt_t::REQUESTON, std::time(nullptr), (int64_t)_fanspeed, _feerate, _totalfee);

		_ctemp = ct;
		_state = state_t::SERVICE;

		bool off = false;
		while (!off)
		{
			std::this_thread::sleep_for(std::chrono::seconds{ 60 });

			state_t cs = state_t::INVALID;
			_slocker.lock();
			cs = _state;
			_period++;
			_slocker.unlock();

			switch (cs)
			{
			case Room::state_t::SERVICE:
			case Room::state_t::SUSPEND:
				_tlocker.lock();
				if (std::fabs(_ctemp - _ttemp) > 0.5)
				{
					_flocker.lock();
					_totalfee += _feerate;
					_flocker.unlock();
				}
				_tlocker.unlock();
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
		SETPARAM,
		STARTUP,
		SHUTDOWN,
		MONITOR
	};

	Room::mode_t defmode;
	double_t deftemp{ 0.0 };
	Room::speed_t deffanspeed{ Room::speed_t::LOW };
	double_t htemp{ 0.0 };
	double_t ltemp{ 0.0 };
	std::map<Room::speed_t, double_t> frate;

	state_t state{ state_t::OFF };
	opt_t opt{ opt_t::IDLE };

	time_t start{ 0 };
	time_t duration{ 0 };
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

	enum class rtype_t
	{
		INVALID,
		DAY,
		WEEK,
		MONTH,
		YEAR
	};
};

struct ACUsr
{
	std::list<Room*> rooms;
	Admin admin;
	Rpt rpt;
	Mgr mgr;
};

class ACSObj
{
public:
	ACSObj(Room& r, Room::speed_t fs) :
		room(r), fanspeed(fs), timestamp(std::time(nullptr)),
		_running(true), _sid(std::bind(&ACSObj::_service, this)), _dlocker(),
		_duration(0)
	{
		r.SetState(Room::state_t::SERVICE);
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

private:
	bool _running;
	std::thread _sid;
	std::mutex _dlocker;
	time_t _duration;

public:
	void Serve(double_t ctemp) { room.On(ctemp); }
	time_t GetDuration() { _dlocker.lock(); time_t dr = _duration; _dlocker.unlock(); return dr; }

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
	ACWObj(Room& r, Room::speed_t tf, time_t delay) :
		room(r), tfanspeed(tf),
		timestamp(std::time(nullptr)), duration(delay)
	{
		r.SetState(Room::state_t::SUSPEND);
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

	bool _onstartup;
	std::thread _ccontroller;
	void _check();
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