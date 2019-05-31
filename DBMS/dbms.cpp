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
				(SQLWCHAR*)"ACSystem", // 数据源
				SQL_NTS,
				(SQLWCHAR*)"sa", // 用户名
				SQL_NTS,
				(SQLWCHAR*)"19981031", // 密码
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
	std::thread{ std::bind(&ACDbms::_insert, this, obj, data) }.detach();
}

struct invoice_t
{
	int64_t roomid;
	int64_t rqtime;
	int64_t rqduration;
	int64_t fanspeed;
	double_t feerate;
	double_t totalfee;
};

struct report_t
{
	int64_t roomid;
	int64_t onoff;
	int64_t ontime;
	int64_t offtime;
	double_t totalfee;
	int64_t dptcount;
	int64_t rdrcount;
	int64_t stpcount;
	int64_t sfscount;
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

		wsprintf(sql,
			U("select roomid,totalfee from %ls where roomid=%ld and ontime>=%ld and offtime<=%ld"),
			U("report"),
			roomid,
			datein,
			dateout
		);

		SQLAllocStmt(_con, &stm);
		SQLExecDirect(stm, sql, SQL_NTS);
		SQLBindCol(
			stm, 1, SQL_BIGINT,
			&(bill.roomid), 1,
			NULL
		);
		SQLRETURN ret = SQLBindCol(
			stm, 2, SQL_FLOAT,
			&(bill.totalfee), 1,
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
		SQLFreeStmt(stm, SQL_DROP);
		data = json::value::array(records);
	}
		break;
	case ACDbms::obj_t::INVOICE:
	{
		std::vector<json::value> records;
		invoice_t invoice;

		wsprintf(sql,
			U("select * from %ls where roomid=%ld and rqtime>=%ld and rqtime<=%ld"),
			U("invoice"),
			roomid,
			datein,
			dateout
		);

		SQLAllocStmt(_con, &stm);
		SQLExecDirect(stm, sql, SQL_NTS);
		SQLBindCol(
			stm, 1, SQL_BIGINT,
			&(invoice.roomid), 1,
			NULL
		);
		SQLBindCol(
			stm, 2, SQL_BIGINT,
			&(invoice.rqtime), 1,
			NULL
		);
		SQLBindCol(
			stm, 3, SQL_BIGINT,
			&(invoice.rqduration), 1,
			NULL
		);
		SQLBindCol(
			stm, 4, SQL_BIGINT,
			&(invoice.fanspeed), 1,
			NULL
		);
		SQLBindCol(
			stm, 5, SQL_FLOAT,
			&(invoice.feerate), 1,
			NULL
		);
		SQLRETURN ret = SQLBindCol(
			stm, 6, SQL_FLOAT,
			&(invoice.totalfee), 1,
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
				add[U("RquestDuration")] = json::value::number(invoice.rqduration);
				add[U("FanSpeed")] = json::value::number(invoice.fanspeed);
				add[U("FeeRate")] = json::value::number(invoice.feerate);
				add[U("Fee")] = json::value::number(invoice.totalfee);

				records.push_back(add);
			}
		}
		SQLFreeStmt(stm, SQL_DROP);
		data = json::value::array(records);
	}
		break;
	case ACDbms::obj_t::REPORT:
	{
		std::vector<json::value> records;
		report_t report;

		wsprintf(sql,
			U("select * from %ls where roomid=%ld and ontime>=%ld and offtime<=%ld"),
			U("report"),
			roomid,
			datein,
			dateout
		);

		SQLAllocStmt(_con, &stm);
		SQLExecDirect(stm, sql, SQL_NTS);
		SQLBindCol(
			stm, 1, SQL_BIGINT,
			&(report.roomid), 1,
			NULL
		);
		SQLBindCol(
			stm, 2, SQL_BIGINT,
			&(report.onoff), 1,
			NULL
		);
		SQLBindCol(
			stm, 3, SQL_BIGINT,
			&(report.ontime), 1,
			NULL
		);
		SQLBindCol(
			stm, 4, SQL_BIGINT,
			&(report.offtime), 1,
			NULL
		);
		SQLBindCol(
			stm, 5, SQL_FLOAT,
			&(report.totalfee), 1,
			NULL
		);
		SQLBindCol(
			stm, 6, SQL_BIGINT,
			&(report.dptcount), 1,
			NULL
		);
		SQLBindCol(
			stm, 7, SQL_BIGINT,
			&(report.rdrcount), 1,
			NULL
		);
		SQLBindCol(
			stm, 8, SQL_BIGINT,
			&(report.stpcount), 1,
			NULL
		);
		SQLRETURN ret = SQLBindCol(
			stm, 9, SQL_BIGINT,
			&(report.sfscount), 1,
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
			data[U("rqtime")].as_integer(),
			data[U("rqduration")].as_integer(),
			data[U("fanspeed")].as_integer(),
			data[U("feerate")].as_double(),
			data[U("totalfee")].as_double()
		};
		wsprintf(sql, U("insert into %ls values(%ld,%ld,%ld,%ld,%.2lf,%.2lf)"), U("invoice"),
			invoice.roomid,
			invoice.rqtime,
			invoice.rqduration,
			invoice.fanspeed,
			invoice.feerate,
			invoice.totalfee
		);

		SQLAllocStmt(_con, &stm);
		SQLExecDirect(stm, sql, SQL_NTS);
		SQLFreeStmt(stm, SQL_DROP);
	}
		break;
	case ACDbms::obj_t::REPORT:
	{
		report_t report{
			data[U("roomid")].as_integer(),
			data[U("onoff")].as_integer(),
			data[U("ontime")].as_integer(),
			data[U("offtime")].as_integer(),
			data[U("totalfee")].as_double(),
			data[U("dptcount")].as_integer(),
			data[U("rdrcount")].as_integer(),
			data[U("stpcount")].as_integer(),
			data[U("sfscount")].as_integer()
		};
		wsprintf(sql, U("insert into %ls values(%ld,%ld,%ld,%ld,%.2lf,%ld,%ld,%ld,%ld)"), U("report"),
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

		SQLAllocStmt(_con, &stm);
		SQLExecDirect(stm, sql, SQL_NTS);
		SQLFreeStmt(stm, SQL_DROP);
	}
		break;
	default:
		break;
	}
}
