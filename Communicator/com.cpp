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
	_running(false), _replycontroller(), _semophare(nullptr)
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

		_semophare = CreateSemaphore(NULL, 0, 0xFF, NULL);
	}
}

ACCom::~ACCom()
{
	CloseHandle(_semophare);

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
		++exist;
	}
	int64_t handler = conv((int64_t)(*exist).first);

	return handler;
}

bool ACCom::PushMessage(const ACMessage& message)
{
	_pushlocker.lock();

	_pushs.push(message);
	ReleaseSemaphore(_semophare, 0x1, NULL);

	_pushlocker.unlock();
	return true;
}

ACMessage ACCom::PullMessage()
{
	_pulllocker.lock();
	ACMessage message = _pulls.front();
	_pulls.pop();
	_pulllocker.unlock();
	return message;
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

void ACCom::_handle_get(http_request message)
{
	//ucout << message.to_string() << std::endl;
	//ucout << L"URI: " << message.relative_uri().to_string() << std::endl;
	//ucout << L"Query: " << message.relative_uri().query() << std::endl;

	auto queries = http::uri::split_query(http::uri::decode(message.relative_uri().query()));
	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	try
	{
		if (paths[0].compare(L"api") == 0)
		{
			if (paths[1].compare(L"ac") == 0)
			{

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
	auto queries = http::uri::split_query(http::uri::decode(message.relative_uri().query()));
	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	try
	{
		if (paths[0].compare(L"api") == 0)
		{
			if (paths[1].compare(L"ac") == 0)
			{
				_tlocker.lock();
				auto sender = std::find_if(_tokens.begin(), _tokens.end(), 
					[&paths](const std::pair<LPToken, std::list<http_request>>& cur) {
					return (*cur.first).compare(paths[2]) == 0;
				});
				int64_t token = conv((int64_t)(*sender).first);
				(*sender).second.push_back(std::move(message));
				_tlocker.unlock();

				_pulllocker.lock();
				_pulls.push(ACMessage{ token, ACMsgType::REQUESTON, message.relative_uri().query() });
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
	ucout << message.to_string() << std::endl;

	std::wstring rep = U("WRITE YOUR OWN DELETE OPERATION");
	message.reply(status_codes::OK, rep);
	return;
}

void ACCom::_handle_delete(http_request message)
{
	ucout << message.to_string() << std::endl;
	std::wstring rep = U("WRITE YOUR OWN PUT OPERATION");
	message.reply(status_codes::OK, rep);
	return;
}

void ACCom::_reply()
{
	ACMessage out;
	while (_running)
	{
		out.type = ACMsgType::INVALID;
		WaitForSingleObject(_semophare, 1000);
		
		_pushlocker.lock();
		if (!_pushs.empty())
		{
			out = _pushs.front();
			_pushs.pop();
		}
		_pushlocker.unlock();

		// TODO
		LPToken token = nullptr;
		switch (out.type)
		{
		case ACMsgType::REQUESTON:
		case ACMsgType::WAIT:
		{
			token = (LPToken)conv(out.token);
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

				json::value msg;
				switch (out.type)
				{
				case ACMsgType::REQUESTON:
				{
					int64_t ttemp;
					double_t fr;
					double_t tf;
					swscanf(out.info.c_str(),
						L"State=ok\nTargetTemp=%lld\nFeerate=%lf\nFee=%lf\n",
						&ttemp, &fr, &tf
					);

					msg.parse["State"] = json::value("ok");
					msg.parse["TargetTemp"] = json::value(ttemp);
					msg.parse["Feerate"] = json::value(fr);
					msg.parse["Fee"] = json::value(tf);
					rep.reply(status_codes::OK, msg)
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
					break;
				case ACMsgType::WAIT:
					msg.parse["State"] = json::value("wait");
					rep.reply(status_codes::OK, msg)
						.then([](pplx::task<void> t)
					{
						try {
							t.get();
						}
						catch (...) {
							//
						}
					});
					break;
				default:
					break;
				}
			
			}
			catch (...)
			{
				_tlocker.unlock();
			}
		}
			break;
		default:
			break;
		}
	}
}
