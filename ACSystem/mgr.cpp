#include "pch.h"
#include "system.h"

void ACSystem::_mgr(const ACMessage& msg)
{
	switch (msg.type)
	{
	case ACMsgType::FETCHREPORT:
		if (msg.body.has_field(U("RoomId"))
			&& msg.body.has_field(U("TypeReport"))
			&& msg.body.has_field(U("Date")))
		{
			_fetchreport(
				msg.body.at(U("RoomId")).as_integer(),
				(Mgr::rtype_t)msg.body.at(U("TypeReport")).as_integer(),
				(int64_t)msg.body.at(U("Date")).as_double()
			);
		}
		break;
	default:
		break;
	}
}

void ACSystem::_fetchreport(int64_t roomid, Mgr::rtype_t rt, time_t head)
{
	wchar_t rid[0xF];
	std::swprintf(rid, U("%I64d"), roomid);
	_log.Log(_log.Time().append(U("Manager requests to check reports of ")).append(rid).append(U(".")));

	_dlocker.lock();
	_usr.mgr.latest = std::time(nullptr);
	int64_t handler = _usr.mgr.handler;
	json::value msg;
	auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&roomid](const Room* cur) {
		return cur->id == roomid;
	});
	if (room != _usr.rooms.end())
	{
		switch (rt)
		{
		case Mgr::rtype_t::DAY:
			msg = (*room)->report.Load(head, head + _usr.mgr.day);
			break;
		case Mgr::rtype_t::WEEK:
			msg = (*room)->report.Load(head, head + _usr.mgr.week);
			break;
		case Mgr::rtype_t::MONTH:
			msg = (*room)->report.Load(head, head + _usr.mgr.month);
			break;
		case Mgr::rtype_t::YEAR:
			msg = (*room)->report.Load(head, head + _usr.mgr.year);
			break;
		default:
			break;
		}
	}
	_dlocker.unlock();

	_com.PushMessage(ACMessage{ handler, ACMsgType::FETCHREPORT, msg });
}