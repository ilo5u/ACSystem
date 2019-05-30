#include "stdafx.h"
#include "log.h"

ACLog::ACLog() :
	_onlogging(false),
	_maxn(0x80), _buffer(),
	_flushout(), _onflushing(false)
{
}

ACLog::~ACLog()
{
}

void ACLog::Start()
{
	_onlogging = true;
}

void ACLog::Shutdown()
{
	_onlogging = false;

	if (_buffer.size() > 0)
	{
		while (_onflushing);
		std::swap(_buffer, _flushout);

		_onflushing = true;
		std::thread{ std::bind(&ACLog::_persistence, this) };
	}
}

std::wstring ACLog::Time()
{
	SYSTEMTIME st = { 0 };
	GetLocalTime(&st);
	WCHAR ct[0xFF];
	wsprintf(ct, L"%d-%02d-%02d %02d:%02d:%02d ",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond
	);
	return ct;
}

bool ACLog::Log(const std::wstring& info)
{
	if (!_onlogging)
		return false;

	if (_buffer.size() == _maxn)
	{
		while (_onflushing);
		std::swap(_buffer, _flushout);

		_onflushing = true;
		std::thread{ std::bind(&ACLog::_persistence, this) };
	}
	return true;
}

static const std::wstring prefix = L"record_%d.log";
void ACLog::_persistence()
{
	wchar_t filename[MAX_PATH];
	std::wofstream logger;

	int seq = 0;
	do
	{
		wsprintf(filename, prefix.c_str(), ++seq);
		logger.open(filename, std::wofstream::app);
	} while (!logger);

	std::for_each(_flushout.begin(), _flushout.end(), [&logger](const std::wstring& out) {
		logger << out.c_str() << std::endl;
	});

	_flushout.clear();

	_onflushing = false;
}
