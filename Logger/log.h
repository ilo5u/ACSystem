#pragma once

class ACLog
{
public:
	ACLog();
	~ACLog();

	ACLog(const ACLog&) = delete;
	ACLog(ACLog&&) = delete;
	ACLog& operator=(const ACLog&) = delete;
	ACLog& operator=(ACLog&&) = delete;

public:
	void Start();
	void Shutdown();
	std::wstring Time();

	bool Log(const std::wstring& info);

private:
	std::atomic<bool> _onlogging;
	std::mutex _blocker;
	std::list<std::wstring> _buffer;

	std::thread _fcontroller;
	void _persistence();

	HANDLE _fsemophare;
};