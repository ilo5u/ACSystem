#include "stdafx.h"
#include "log.h"

ACLog::ACLog() :
	_onlogging(false),
	_buffer(), _fcontroller()
{
	_fsemophare = CreateSemaphore(NULL, 0, 0xFF, NULL);
}

ACLog::~ACLog()
{
	Shutdown();

	CloseHandle(_fsemophare);
}

void ACLog::Start()
{
	_onlogging = true;

	_fcontroller = std::move(std::thread{ std::bind(&ACLog::_persistence, this) });
}

void ACLog::Shutdown()
{
	_onlogging = false;

	if (_fcontroller.joinable())
		_fcontroller.join();
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
	ReleaseSemaphore(_fsemophare, 0x1, NULL);
	_blocker.unlock();
	return true;
}

static const std::wstring prefix = L"%d-%d-%d.log";
void ACLog::_persistence()
{
	wchar_t filename[MAX_PATH];
	SYSTEMTIME st = { 0 };

	while (_onlogging)
	{
		WaitForSingleObject(_fsemophare, 2000);

		_blocker.lock();
		if (_buffer.size() > 0)
		{
			GetLocalTime(&st);
			std::swprintf(filename, prefix.c_str(),
				st.wYear,
				st.wMonth,
				st.wDay
			);

			std::wofstream logger(filename, std::wofstream::app | std::wofstream::out);
			std::for_each(_buffer.begin(), _buffer.end(), [&logger](const std::wstring& out) {
				logger << out.c_str() << std::endl;
			});
			logger.close();

			_buffer.clear();
		}
		_blocker.unlock();
	}
}
