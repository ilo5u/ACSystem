#pragma once
#include <sqlext.h>

class ACDbms
{
public:
	ACDbms();
	~ACDbms();

	ACDbms(const ACDbms&) = delete;
	ACDbms(ACDbms&&) = delete;
	ACDbms& operator=(const ACDbms&) = delete;
	ACDbms& operator=(ACDbms&&) = delete;

public:
	enum class obj_t
	{
		BILL,
		INVOICE,
		REPORT
	};

public:
	bool Connect();
	void Disconnect();

	void Insert(obj_t obj, web::json::value data);
	web::json::value Select(obj_t obj, int64_t roomid, time_t datein, time_t dateout);

private:
	std::mutex _protection;
	void _insert(obj_t obj, web::json::value data);

private:
	SQLHENV _env;
	SQLHDBC _con;
};