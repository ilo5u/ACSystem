#include "stdafx.h"
#include "com.h"

#include <Windows.h>

#ifdef _DEBUG
#pragma comment(lib, "x64/Debug/cpprest140d_2_9.lib")
#else
#pragma comment(lib, "x64/Release/cpprest140_2_9.lib")
#endif // _DEBUG


ACCom::ACCom(const std::wstring& address) :
	_tokens(), _listener(nullptr),
	_pulls(), _pulllocker(),
	_pushs(), _pushlocker(),
	_running(false), _replycontroller(), _pushsemophare(nullptr)
{
	uri_builder uri(address);
	_listener = new http_listener(uri.to_uri().to_string());
	if (_listener != nullptr)
	{
		_listener->support(
			methods::GET,
			std::bind(&ACCom::_handle_get, this, std::placeholders::_1)
		);
		_listener->support(
			methods::PUT,
			std::bind(&ACCom::_handle_put, this, std::placeholders::_1)
		);
		_listener->support(
			methods::POST,
			std::bind(&ACCom::_handle_post, this, std::placeholders::_1)
		);
		_listener->support(
			methods::DEL,
			std::bind(&ACCom::_handle_delete, this, std::placeholders::_1)
		);

		_pullsemophare = CreateSemaphore(NULL, 0, 0xFF, NULL);
		_pushsemophare = CreateSemaphore(NULL, 0, 0xFF, NULL);
	}
}

ACCom::~ACCom()
{
	CloseHandle(_pushsemophare);
	CloseHandle(_pullsemophare);

	if (_replycontroller.joinable())
		_replycontroller.join();

	_listener->close().wait();
	delete _listener;
	_listener = nullptr;

	std::for_each(
		_tokens.begin(), _tokens.end(), [](std::pair<LPToken, std::list<http_request>>& cur) {
		delete cur.first;
		cur.first = nullptr;
	});
}

ACCom::Handler ACCom::CreateHandler(const std::wstring& token)
{
	TokenIndics::const_iterator exist = std::find_if(
		_tokens.begin(), _tokens.end(), [&token](const std::pair<LPToken, std::list<http_request>>& cur) {
		return token == *(cur.first);
	});

	if (exist == _tokens.end())
	{
		_tokens.push_back({ new Token{ token }, {} });
		exist = std::find_if(
			_tokens.begin(), _tokens.end(), [&token](const std::pair<LPToken, std::list<http_request>>& cur) {
			return token == *(cur.first);
		});
	}

	return conv((int64_t)(*exist).first);
}

bool ACCom::PushMessage(const ACMessage& message)
{
	_pushlocker.lock();

	_pushs.push(message);
	ReleaseSemaphore(_pushsemophare, 0x1, NULL);

	_pushlocker.unlock();
	return true;
}

ACMessage ACCom::PullMessage()
{
	ACMessage in{ 0, ACMsgType::INVALID, {} };
	WaitForSingleObject(_pullsemophare, 1000);

	_pulllocker.lock();
	if (!_pulls.empty())
	{
		in = _pulls.front();
		_pulls.pop();
	}
	_pulllocker.unlock();
	return in;
}

pplx::task<void> ACCom::Start()
{
	_running = true;
	_replycontroller = std::move(std::thread{ std::bind(&ACCom::_reply, this) });
	return _listener->open();
}

pplx::task<void> ACCom::Shutdown()
{
	return _listener->close();
}

int64_t ACCom::_fetch(const Token& token, http_request& message)
{
	_tlocker.lock();
	auto sender = std::find_if(_tokens.begin(), _tokens.end(),
		[&token](const std::pair<LPToken, std::list<http_request>>& cur) {
		return (*cur.first).compare(token) == 0;
	});
	if (sender != _tokens.end())
	{
		int64_t handler = conv((int64_t)(*sender).first);
		(*sender).second.push_back(std::move(message));
		_tlocker.unlock();

		return handler;
	}
	else
	{
		return 0;
	}
}

void ACCom::_handle_get(http_request message)
{
	//ucout << message.to_string() << std::endl;
	//ucout << L"URI: " << message.relative_uri().to_string() << std::endl;
	//ucout << L"Query: " << message.relative_uri().query() << std::endl;

	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	json::value body = message.extract_json().get();
	try
	{
		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("power")))
			{
				int64_t handler = _fetch(U("Admin"), message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::STARTUP, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("state")))
			{
				int64_t handler = _fetch(U("Admin"), message);
				//int64_t rid = 0;
				//swscanf(paths[2].c_str(), U("%ld"), &rid);
				//body[U("RoomId")] = json::value::number(rid);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::MONITOR, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("ac")) == 0)
			{

				int64_t handler = _fetch(paths[2], message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::FETCHFEE, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
		}
		else
		{

		}
	}
	catch (...)
	{

	}

	//Dbms* d  = new Dbms();
	//d->connect();
	//concurrency::streams::fstream::open_istream(U("static/index.html"), std::ios::in).then([=](concurrency::streams::istream is)
	//{
	//	message.reply(status_codes::OK, is, U("text/html"))
	//		.then([](pplx::task<void> t)
	//	{
	//		try {
	//			t.get();
	//		}
	//		catch (...) {
	//			//
	//		}
	//	});
	//}).then([=](pplx::task<void>t)
	//{
	//	try {
	//		t.get();
	//	}
	//	catch (...) {
	//		message.reply(status_codes::InternalError, U("INTERNAL ERROR "));
	//	}
	//});

	return;
}

void ACCom::_handle_put(http_request message)
{
	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	json::value body = message.extract_json().get();
	try
	{
		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("ac")) == 0)
			{
				if (paths[2].compare(U("notify")) == 0)
				{
					int64_t handler = _fetch(paths[3], message);

					_pulllocker.lock();
					_pulls.push(ACMessage{ handler, ACMsgType::TEMPNOTIFICATION, body });
					ReleaseSemaphore(_pullsemophare, 1, NULL);
					_pulllocker.unlock();
				}
				else
				{
					int64_t handler = _fetch(paths[2], message);

					_pulllocker.lock();
					_pulls.push(ACMessage{ handler, ACMsgType::REQUESTON, body });
					ReleaseSemaphore(_pullsemophare, 1, NULL);
					_pulllocker.unlock();
				}
			}
			else if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::POWERON, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
		}
		else
		{

		}
	}
	catch (...)
	{

	}
	return;
}

void ACCom::_handle_post(http_request message)
{
	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	json::value body = message.extract_json().get();
	try
	{
		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::SETPARAM, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("ac")) == 0)
			{
				int64_t handler = _fetch(paths[2], message);
				if (body.has_field(U("TargetTemp")))
				{
					_pulllocker.lock();
					_pulls.push(ACMessage{ handler, ACMsgType::SETTEMP, body });
					ReleaseSemaphore(_pullsemophare, 1, NULL);
					_pulllocker.unlock();
				}
				else if (body.has_field(U("FanSpeed")))
				{
					_pulllocker.lock();
					_pulls.push(ACMessage{ handler, ACMsgType::SETFANSPEED, body });
					ReleaseSemaphore(_pullsemophare, 1, NULL);
					_pulllocker.unlock();
				}
			}
			
		}
		else
		{

		}
	}
	catch (...)
	{

	}
	return;
}

void ACCom::_handle_delete(http_request message)
{
	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	json::value body = message.extract_json().get();
	try
	{
		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::SHUTDOWN, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("ac")) == 0)
			{
				int64_t handler = _fetch(paths[2], message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::REQUESTOFF, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
		}
		else
		{

		}
	}
	catch (...)
	{

	}
	return;
}

void ACCom::_reply()
{
	ACMessage out;
	while (_running)
	{
		out.type = ACMsgType::INVALID;
		WaitForSingleObject(_pushsemophare, 1000);
		
		_pushlocker.lock();
		if (!_pushs.empty())
		{
			out = _pushs.front();
			_pushs.pop();
		}
		_pushlocker.unlock();

		if (out.type != ACMsgType::INVALID)
		{
			LPToken token = (LPToken)conv(out.token);
			_tlocker.lock();
			auto recver = std::find_if(_tokens.begin(), _tokens.end(),
				[&token](const std::pair<LPToken, std::list<http_request>>& cur) {
				return cur.first == token;
			});
			try
			{
				http_request rep = std::move((*recver).second.front());
				(*recver).second.erase((*recver).second.begin());
				_tlocker.unlock();

				rep.reply(status_codes::OK, out.body)
					.then([](pplx::task<void> t)
				{
					try {
						t.get();
					}
					catch (...) {
						//
					}
				});

			}
			catch (...)
			{
				_tlocker.unlock();
			}
		}
	}
}
