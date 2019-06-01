#include "pch.h"
#include "system.h"

void ACSystem::_rpt(const ACMessage& msg)
{
	switch (msg.type)
	{
	case ACMsgType::FETCHBILL:
		if (msg.body.has_field(U("RoomId"))
			&& msg.body.has_field(U("DateIn"))
			&& msg.body.has_field(U("DateOut")))
		{
			_fetchbill(
				msg.body.at(U("RoomId")).as_integer(),
				msg.body.at(U("DateIn")).as_integer(),
				msg.body.at(U("DateOut")).as_integer()
			);
		}
		break;
	case ACMsgType::FETCHINVOICE:
		if (msg.body.has_field(U("RoomId"))
			&& msg.body.has_field(U("DateIn"))
			&& msg.body.has_field(U("DateOut")))
		{
			_fetchinvoice(
				msg.body.at(U("RoomId")).as_integer(),
				msg.body.at(U("DateIn")).as_integer(),
				msg.body.at(U("DateOut")).as_integer()
			);
		}
		break;
	default:
		break;
	}
}

void ACSystem::_fetchbill(int64_t roomid, time_t din, time_t dout)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), roomid);

	_usr.rpt.rquestcnt++;
	_log.Log(_log.Time().append(U("Reception requests to check the bill of ")).append(rid).append(U(".")));

	_usr.rpt.latest = std::time(nullptr);

	json::value msg;
	try
	{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&roomid](const Room* cur) {
			return cur->id == roomid;
		});
		msg = (*room)->bill.Load(din, dout);
		(*room)->rdrcount++;
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ _usr.rpt.handler, ACMsgType::FETCHBILL, msg });
	_usr.rpt.rponsecnt++;
}

void ACSystem::_fetchinvoice(int64_t roomid, time_t din, time_t dout)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), roomid);

	_usr.rpt.rquestcnt++;
	_log.Log(_log.Time().append(U("Reception requests to check the bill of ")).append(rid).append(U(".")));

	_usr.rpt.latest = std::time(nullptr);

	json::value msg;
	try
	{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&roomid](const Room* cur) {
			return cur->id == roomid;
		});
		msg = (*room)->invoice.Load(din, dout);
	}
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ _usr.rpt.handler, ACMsgType::FETCHINVOICE, msg });
	_usr.rpt.rponsecnt++;
}