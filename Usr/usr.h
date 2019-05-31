#pragma once

struct UBase
{
	ACCom::Handler handler{ 0 };

	time_t latest{ 0 };
	time_t period{ 0 };
	int64_t rquestcnt{ 0 };
	int64_t rponsecnt{ 0 };
	bool inservice{ false };
};

class Room;
class Bill
{
public:
	Bill(Room* room, ACDbms& dbms);

public:
	json::value Load(time_t datein, time_t dateout);

private:
	Room* _room;
	ACDbms& _dbms;
};

class Report
{
public:
	Report(Room* room, ACDbms& dbms);

public:
	void Store(int64_t onoff, time_t ontime, time_t offtime, double_t totalfee, 
		int64_t dptcount, int64_t rdrcount, int64_t stpcount, int64_t sfscount);
	json::value Load(time_t datein, time_t dateout);

private:
	Room* _room;
	ACDbms& _dbms;
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

public:
	Invoice(Room* room, ACDbms& dbms);

public:
	void Prepare(opt_t opt, time_t request);
	void Store(opt_t opt, time_t response, int64_t fanspeed, double_t feerate, double_t fee);
	json::value Load(time_t datein, time_t dateout);

private:
	std::mutex _plocker;
	std::map<opt_t, time_t> _prep;

	Room* _room;
	ACDbms& _dbms;
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
	Room(int64_t roomid, ACDbms& dbms);
	~Room();

public:
	int64_t id{ 0 };
	opt_t opt{ opt_t::IDLE };

	mode_t mode{ mode_t::COOL };
	std::atomic<double_t> ctemp{ 26.0 };

	std::atomic<state_t> state{ state_t::STOPPED };
	std::atomic<time_t> ontime{ 0 };
	std::atomic<time_t> duration{ 0 };

	std::atomic<int64_t> dptcount{ 0 };
	std::atomic<int64_t> rdrcount{ 0 };
	std::atomic<int64_t> stpcount{ 0 };
	std::atomic<int64_t> sfscount{ 0 };

	Bill bill;
	Invoice invoice;
	Report report;

private:
	std::atomic<double_t> _ttemp{ 28.0 };
	speed_t _fanspeed{ speed_t::LOW };
	double_t _feerate{ 1.5 };
	double_t _totalfee{ 0.0 };

	std::thread _charging;
	std::mutex _flocker;

public:
	void On(double_t ct);

public:
	speed_t GetFanspeed();
	double_t GetTargetTemp();
	double_t GetTotalfee();

public:
	void SetFanspeed(speed_t fs, double_t fr);
	void SetTargetTemp(double_t tt);
	void Reset();

private:
	void _on(double_t ct);
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

	Room::mode_t defmode{ Room::mode_t::COOL };
	double_t deftemp{ 26.0 };
	Room::speed_t deffanspeed{ Room::speed_t::LOW };
	double_t htemp{ 30.0 };
	double_t ltemp{ 18.0 };
	std::map<Room::speed_t, double_t> frate;

	state_t state{ state_t::READY };
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

	const time_t day = 60 * 60 * 24;
	const time_t week = 60 * 60 * 24 * 7;
	const time_t month = 60 * 60 * 24 * 30;
	const time_t year = 60 * 60 * 24 * 365;
};

struct ACUsr
{
	std::list<Room*> rooms;
	Admin admin;
	Rpt rpt;
	Mgr mgr;
};