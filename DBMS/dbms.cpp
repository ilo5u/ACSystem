#include "stdafx.h"
#include "dbms.h"

using namespace web;

ACDbms::ACDbms() :
	_env(NULL), _con(NULL)
{
}

ACDbms::~ACDbms()
{
	Disconnect();
}

bool ACDbms::Connect()
{
	SQLRETURN ret = SQLAllocEnv(&_env);	// 初始化SQL环境
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
	{
		ret = SQLAllocConnect(_env, &_con);	// 分配连接句柄
		if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		{
			ret = SQLConnect(
				_con,
				(SQLWCHAR*)U("ACSystem"), // 数据源
				SQL_NTS,
				(SQLWCHAR*)U("sa"), // 用户名
				SQL_NTS,
				(SQLWCHAR*)U("19981031"), // 密码
				SQL_NTS
			);

			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				return true;
			}
			SQLFreeConnect(_con);
		}
		SQLFreeEnv(_env);
	}
	return false;
}

void ACDbms::Disconnect()
{
	if (_con != NULL)
	{
		SQLDisconnect(_con);
		SQLFreeConnect(_con);
		_con = NULL;
	}

	if (_env != NULL)
	{
		SQLFreeEnv(_env);
		_env = NULL;
	}
}

void ACDbms::Insert(obj_t obj, json::value data)
{
	// std::thread{ std::bind(&ACDbms::_insert, this, obj, data) }.detach();
	_insert(obj, data);
}

struct invoice_t
{
	SQLBIGINT roomid;
	SQLBIGINT rqtime;
	SQLBIGINT rqduration;
	SQLBIGINT fanspeed;
	SQLFLOAT feerate;
	SQLFLOAT totalfee;
};

struct report_t
{
	SQLBIGINT roomid;
	SQLBIGINT onoff;
	SQLBIGINT ontime;
	SQLBIGINT offtime;
	SQLFLOAT totalfee;
	SQLBIGINT dptcount;
	SQLBIGINT rdrcount;
	SQLBIGINT stpcount;
	SQLBIGINT sfscount;
};

struct bill_t
{
	int64_t roomid;
	int64_t ontime;
	int64_t offtime;
	double_t totalfee;
};

json::value ACDbms::Select(obj_t obj, int64_t roomid, time_t datein, time_t dateout)
{
	wchar_t sql[0xFF];
	SQLHSTMT stm;
	json::value data;
	switch (obj)
	{
	case ACDbms::obj_t::BILL:
	{
		std::vector<json::value> records;
		bill_t bill;

		std::swprintf(sql,
			U("select roomid,totalfee from %ls where roomid=%I64d and ontime>=%I64d and offtime<=%I64d"),
			U("report"),
			roomid,
			datein,
			dateout
		);

		_protection.lock();

		SQLRETURN ret = SQLAllocStmt(_con, &stm);
		ret = SQLExecDirect(stm, sql, SQL_NTS);
		ret = SQLBindCol(
			stm, 1, SQL_C_SBIGINT,
			&(bill.roomid), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 2, SQL_C_DOUBLE,
			&(bill.totalfee), 0,
			NULL
		);

		while (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		{
			ret = SQLFetch(stm);
			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				json::value add;
				add[U("RoomId")] = json::value::number(bill.roomid);
				add[U("TotalFee")] = json::value::number(bill.totalfee);

				records.push_back(add);
			}
		}
		ret = SQLFreeStmt(stm, SQL_DROP);

		_protection.unlock();

		data = json::value::array(records);
	}
		break;
	case ACDbms::obj_t::INVOICE:
	{
		std::vector<json::value> records;
		invoice_t invoice;

		std::swprintf(sql,
			U("select * from %ls where roomid=%I64d and rqtime>=%I64d and rqtime<=%I64d"),
			U("invoice"),
			roomid,
			datein,
			dateout
		);

		_protection.lock();

		SQLRETURN ret = SQLAllocStmt(_con, &stm);
		ret = SQLExecDirect(stm, sql, SQL_NTS);
		ret = SQLBindCol(
			stm, 1, SQL_C_SBIGINT,
			&invoice.roomid, 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 2, SQL_C_SBIGINT,
			&(invoice.rqtime), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 3, SQL_C_SBIGINT,
			&(invoice.rqduration), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 4, SQL_C_SBIGINT,
			&(invoice.fanspeed), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 5, SQL_C_DOUBLE,
			&(invoice.feerate), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 6, SQL_C_DOUBLE,
			&(invoice.totalfee), 0,
			NULL
		);

		while (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		{
			ret = SQLFetch(stm);
			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				json::value add;
				add[U("RoomId")] = json::value::number(invoice.roomid);
				add[U("RequestTime")] = json::value::number(invoice.rqtime);
				add[U("RequestDuration")] = json::value::number(invoice.rqduration);
				add[U("FanSpeed")] = json::value::number(invoice.fanspeed);
				add[U("FeeRate")] = json::value::number(invoice.feerate);
				add[U("Fee")] = json::value::number(invoice.totalfee);

				records.push_back(add);
			}
		}
		ret = SQLFreeStmt(stm, SQL_DROP);

		_protection.unlock();
		data = json::value::array(records);
	}
		break;
	case ACDbms::obj_t::REPORT:
	{
		std::vector<json::value> records;
		report_t report;

		std::swprintf(sql,
			U("select * from %ls where roomid=%I64d and ontime>=%I64d and offtime<=%I64d"),
			U("report"),
			roomid,
			datein,
			dateout
		);

		_protection.lock();

		SQLRETURN ret = SQLAllocStmt(_con, &stm);
		ret = SQLExecDirect(stm, sql, SQL_NTS);
		ret = SQLBindCol(
			stm, 1, SQL_C_SBIGINT,
			&(report.roomid), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 2, SQL_C_SBIGINT,
			&(report.onoff), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 3, SQL_C_SBIGINT,
			&(report.ontime), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 4, SQL_C_SBIGINT,
			&(report.offtime), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 5, SQL_C_DOUBLE,
			&(report.totalfee), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 6, SQL_C_SBIGINT,
			&(report.dptcount), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 7, SQL_C_SBIGINT,
			&(report.rdrcount), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 8, SQL_C_SBIGINT,
			&(report.stpcount), 0,
			NULL
		);
		ret = SQLBindCol(
			stm, 9, SQL_C_SBIGINT,
			&(report.sfscount), 0,
			NULL
		);

		while (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		{
			ret = SQLFetch(stm);
			if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			{
				json::value add;
				add[U("RoomId")] = json::value::number(report.roomid);
				add[U("TimesOfOnOff")] = json::value::number(report.onoff);
				add[U("Duration")] = json::value::number(report.offtime - report.ontime);
				add[U("TotalFee")] = json::value::number(report.totalfee);
				add[U("TimesOfDispatch")] = json::value::number(report.dptcount);
				add[U("NumberOfRDR")] = json::value::number(report.rdrcount);
				add[U("TimesOfChangeTemp")] = json::value::number(report.stpcount);
				add[U("TimesOfChangeFanSpeed")] = json::value::number(report.sfscount);

				records.push_back(add);
			}
		}
		SQLFreeStmt(stm, SQL_DROP);

		_protection.unlock();

		data = json::value::array(records);
	}
		break;
	default:
		break;
	}
	return data;
}

void ACDbms::_insert(obj_t obj, json::value data)
{
	wchar_t sql[0xFF];
	SQLHSTMT stm;
	switch (obj)
	{
	case ACDbms::obj_t::INVOICE:
	{
		invoice_t invoice{
			data[U("roomid")].as_integer(),
			(int64_t)data[U("rqtime")].as_double(),
			(int64_t)data[U("rqduration")].as_double(),
			data[U("fanspeed")].as_integer(),
			data[U("feerate")].as_double(),
			data[U("totalfee")].as_double()
		};
		std::swprintf(sql, U("insert into %ls values(%I64d,%I64d,%I64d,%I64d,%.2lf,%.2lf)"), U("invoice"),
			invoice.roomid,
			invoice.rqtime,
			invoice.rqduration,
			invoice.fanspeed,
			invoice.feerate,
			invoice.totalfee
		);
		_protection.lock();

		SQLRETURN ret = SQLAllocStmt(_con, &stm);
		ret = SQLExecDirect(stm, sql, SQL_NTS);
		ret = SQLFreeStmt(stm, SQL_DROP);

		_protection.unlock();
	}
		break;
	case ACDbms::obj_t::REPORT:
	{
		report_t report{
			data[U("roomid")].as_integer(),
			data[U("onoff")].as_integer(),
			(int64_t)data[U("ontime")].as_double(),
			(int64_t)data[U("offtime")].as_double(),
			data[U("totalfee")].as_double(),
			data[U("dptcount")].as_integer(),
			data[U("rdrcount")].as_integer(),
			data[U("stpcount")].as_integer(),
			data[U("sfscount")].as_integer()
		};
		std::swprintf(sql, U("insert into %ls values(%I64d,%I64d,%I64d,%I64d,%.2lf,%I64d,%I64d,%I64d,%I64d)"), U("report"),
			report.roomid,
			report.onoff,
			report.ontime,
			report.offtime,
			report.totalfee,
			report.dptcount,
			report.rdrcount,
			report.stpcount,
			report.sfscount
		);

		_protection.lock();

		SQLRETURN ret = SQLAllocStmt(_con, &stm);
		ret = SQLExecDirect(stm, sql, lstrlen(sql));
		ret = SQLFreeStmt(stm, SQL_DROP);

		_protection.unlock();
	}
		break;
	default:
		break;
	}
}
