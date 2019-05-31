#include "pch.h"
#include "system.h"

void ACSystem::_rpt(const ACMessage& msg)
{
	switch (msg.type)
	{
	case ACMsgType::FETCHBILL:
		break;
	case ACMsgType::FETCHINVOICE:
		break;
	default:
		break;
	}
}

void ACSystem::_fetchbill(int64_t roomid, time_t din, time_t dout)
{
}

void ACSystem::_fetchinvoice(int64_t roomid, time_t din, time_t dout)
{
}