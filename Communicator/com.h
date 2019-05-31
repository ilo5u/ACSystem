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
	FETCHFEE,
	TEMPNOTIFICATION,
	POWERON,
	SETPARAM,
	STARTUP,
	SHUTDOWN,
	MONITOR,
	FETCHBILL,
	FETCHINVOICE,
	FETCHREPORT,
	WAIT
};

struct ACMessage
{
	int64_t token;
	ACMsgType type;
	json::value body;
	
	//std::map<utility::string_t, utility::string_t> info;

	ACMessage() :
		token(0), type(ACMsgType::INVALID), body()
	{
	}

	ACMessage(int64_t tk, ACMsgType tp, 
		const json::value& msg) :
		token(tk), type(tp), body(msg)
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
	typedef utility::string_t Token, * LPToken;
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

	int64_t _fetch(const Token& token, http_request& message);

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
	Semophare _pullsemophare;
	Semophare _pushsemophare;
};