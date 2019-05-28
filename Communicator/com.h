#pragma once
using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

typedef HANDLE Semophare;

enum class ACMsgType
{
	INVALID,
	REQUESTON,
	REQUESTOFF,
	SETTEMP,
	SETFANSPEED,
	TEMPNOTIFICATION,
	POWERON,
	POWEROFF,
	MONITOR,
	FETCHBILL,
	FETCHINVOICE,
	FETCHREPORT,
	OK,
	WAIT
};

struct ACMessage
{
	int64_t token;
	ACMsgType type;
	std::wstring info;

	ACMessage() :
		token(0), type(ACMsgType::INVALID), info()
	{
	}

	ACMessage(int64_t tk, ACMsgType tp, const std::wstring& msg) :
		token(tk), type(tp), info(msg)
	{
	}
};

class ACCom
{
public:
	typedef typename int64_t Handler;

public:
	ACCom(const std::wstring& address);
	~ACCom();

	ACCom(const ACCom&) = delete;
	ACCom(ACCom&&) = delete;
	ACCom& operator=(const ACCom&) = delete;
	ACCom& operator=(ACCom&&) = delete;

public:
	Handler CreateHandler(const std::wstring& token);

	bool PushMessage(const ACMessage& message);
	ACMessage PullMessage();

public:
	pplx::task<void> Start();
	pplx::task<void> Shutdown();

private:
	typedef std::wstring Token, * LPToken;
	typedef std::vector<std::pair<LPToken, std::list<http_request>>> TokenIndics;
	TokenIndics _tokens;
	std::mutex _tlocker;

	struct Converter
	{
		int64_t operator()(int64_t old)
		{
			int8_t *byte = (int8_t*)&old;
			for (int i = 0; i < sizeof(int64_t) / 2; ++i)
				std::swap(*(byte + i), *(byte + sizeof(int64_t) - i - 1));
			return old;
		}
	} conv;

private:
	http_listener * _listener;

private:
	void _handle_get(http_request message);
	void _handle_put(http_request message);
	void _handle_post(http_request message);
	void _handle_delete(http_request message);

private:
	std::queue<ACMessage> _pulls;
	std::mutex _pulllocker;

	std::queue<ACMessage> _pushs;
	std::mutex _pushlocker;

	bool _running;
	void _reply();
	std::thread _replycontroller;
	Semophare _semophare;
};