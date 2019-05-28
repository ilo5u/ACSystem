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

	bool Log(const std::wstring& info);

private:
	bool _onlogging;

	const int32_t _maxn;
	std::list<std::wstring> _buffer;
	std::list<std::wstring> _flushout;
	bool _onflushing;
	void _persistence();
};