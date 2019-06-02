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
				(time_t)msg.body.at(U("DateIn")).as_double(),
				(time_t)msg.body.at(U("DateOut")).as_double()
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
				(time_t)msg.body.at(U("DateIn")).as_double(),
				(time_t)msg.body.at(U("DateOut")).as_double()
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
	std::swprintf(rid, U("%I64d"), roomid);
	_log.Log(_log.Time().append(U("Reception requests to check the bill of ")).append(rid).append(U(".")));

	_dlocker.lock();
	int64_t handler = _usr.rpt.handler;
	_usr.rpt.latest = std::time(nullptr);
	json::value msg;
	//try
	//{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&roomid](const Room* cur) {
			return cur->id == roomid;
		});
		if (room != _usr.rooms.end())
		{
			msg = (*room)->bill.Load(din, dout);
			(*room)->rdrcount++;
		}
	//}
	//catch (...)
	//{
	//	_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	//}
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::FETCHBILL, msg });
}

void ACSystem::_fetchinvoice(int64_t roomid, time_t din, time_t dout)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), roomid);
	_log.Log(_log.Time().append(U("Reception requests to check the bill of ")).append(rid).append(U(".")));

	_dlocker.lock();
	int64_t handler = _usr.rpt.handler;
 	_usr.rpt.latest = std::time(nullptr);
	json::value msg;
	//try
	//{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&roomid](const Room* cur) {
			return cur->id == roomid;
		});
		if (room != _usr.rooms.end())
		{
			msg = (*room)->invoice.Load(din, dout);
		}
	//}
	//catch (...)
	//{
	//	_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	//}
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::FETCHINVOICE, msg });
}