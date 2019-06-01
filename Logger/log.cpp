#include "stdafx.h"
#include "log.h"

ACLog::ACLog() :
	_onlogging(false),
	_maxn(0x8), _buffer(),
	_flushout(), _fcontroller()
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
		if (_fcontroller.joinable())
			_fcontroller.join();

		std::swap(_buffer, _flushout);

		_fcontroller = std::move(std::thread{ std::bind(&ACLog::_persistence, this) });
		_fcontroller.join();
	}
}

std::wstring ACLog::Time()
{
	SYSTEMTIME st = { 0 };
	GetLocalTime(&st);
	WCHAR ct[0xFF];
	std::swprintf(ct, L"[%d-%02d-%02d %02d:%02d:%02d] ",
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

	_blocker.lock();
	_buffer.push_back(info);
	if (_buffer.size() > _maxn)
	{
		if (_fcontroller.joinable())
			_fcontroller.join();

		_flushout = _buffer;
		_buffer.clear();

		_fcontroller = std::move(std::thread{ std::bind(&ACLog::_persistence, this) });
	}
	_blocker.unlock();
	return true;
}

static const std::wstring prefix = L"%d-%d-%d.log";
void ACLog::_persistence()
{
	wchar_t filename[MAX_PATH];

	SYSTEMTIME st = { 0 };
	GetLocalTime(&st);
	std::swprintf(filename, prefix.c_str(),
		st.wYear,
		st.wMonth,
		st.wDay
	);

	std::wofstream logger(filename, std::wofstream::app | std::wofstream::out);

	std::for_each(_flushout.begin(), _flushout.end(), [&logger](const std::wstring& out) {
		logger << out.c_str() << std::endl;
	});
	logger.flush();

	_flushout.clear();
}
