#include "pch.h"
#include "system.h"

void ACSystem::_mgr(const ACMessage& msg)
{
	switch (msg.type)
	{
	case ACMsgType::FETCHREPORT:
		_fetchreport(
			msg.body.at(U("RoomId")).as_integer(),
			(Mgr::rtype_t)msg.body.at(U("TypeReport")).as_integer(),
			msg.body.at(U("DateIn")).as_integer()
		);
		break;
	default:
		break;
	}
}

void ACSystem::_fetchreport(int64_t roomid, Mgr::rtype_t rt, time_t head)
{
	wchar_t rid[0xF];
	wsprintf(rid, U("%ld"), roomid);

	_usr.mgr.rquestcnt++;
	_log.Log(_log.Time().append(U("Manager requests to check reports of ")).append(rid).append(U(".")));

	_usr.mgr.latest = std::time(nullptr);

	json::value msg;
	try
	{
		auto room = std::find_if(_usr.rooms.begin(), _usr.rooms.end(), [&roomid](const Room* cur) {
			return cur->id == roomid;
		});

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
	catch (...)
	{
		_log.Log(_log.Time().append(rid).append(U(" Room does not exist.")));
	}

	_com.PushMessage(ACMessage{ _usr.mgr.handler, ACMsgType::FETCHREPORT, msg });
	_usr.mgr.rponsecnt++;
}