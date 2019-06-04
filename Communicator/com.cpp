#include "stdafx.h"
#include "com.h"

#include <Windows.h>

#ifdef _DEBUG
#pragma comment(lib, "x64/Debug/cpprest140d_2_9.lib")
#else
#pragma comment(lib, "x64/Release/cpprest140_2_9.lib")
#endif // _DEBUG


ACCom::ACCom(const std::wstring& address) :
	_tokens(), _tlocker(),
	_listener(nullptr),
	_pulls(), _pulllocker(),
	_pushs(), _pushlocker(),
	_running(false), _replycontroller(),
	_pullsemophare(NULL), _pushsemophare(NULL)
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
		_listener->support(
			methods::OPTIONS,
			std::bind(&ACCom::_handle_options, this, std::placeholders::_1)
		);

		_pullsemophare = CreateSemaphore(NULL, 0, 0x80, NULL);
		_pushsemophare = CreateSemaphore(NULL, 0, 0x80, NULL);
	}
}

ACCom::~ACCom()
{
	CloseHandle(_pushsemophare);
	CloseHandle(_pullsemophare);

	_running = false;
	if (_replycontroller.joinable())
		_replycontroller.join();

	_listener->close().wait();
	delete _listener;
	_listener = nullptr;

	std::for_each(
		_tokens.begin(), _tokens.end(), [](std::pair<LPToken, std::map<method_t, std::list<http_request>>>& cur) {
		delete cur.first;
		cur.first = nullptr;
	});
}

ACCom::Handler ACCom::CreateHandler(const std::wstring& token)
{
	TokenIndics::const_iterator exist = std::find_if(
		_tokens.begin(), _tokens.end(), [&token](std::pair<LPToken, std::map<method_t, std::list<http_request>>>& cur) {
		return token == *(cur.first);
	});

	if (exist == _tokens.end())
	{
		_tokens.push_back({ new Token{ token }, {} });
		exist = std::find_if(
			_tokens.begin(), _tokens.end(), [&token](std::pair<LPToken, std::map<method_t, std::list<http_request>>>& cur) {
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
	WaitForSingleObject(_pullsemophare, 2000);

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

int64_t ACCom::_fetch(const Token& token, method_t method, http_request& message)
{
	_tlocker.lock();
	auto sender = std::find_if(_tokens.begin(), _tokens.end(),
		[&token](std::pair<LPToken, std::map<method_t, std::list<http_request>>>& cur) {
		return (*cur.first).compare(token) == 0;
	});
	if (sender != _tokens.end())
	{
		int64_t handler = conv((int64_t)(*sender).first);
		((*sender).second)[method].push_back(std::move(message));
		_tlocker.unlock();

		return handler;
	}
	else
	{
		_tlocker.unlock();
		return 0;
	}
}

void ACCom::_handle_get(http_request message)
{
	try
	{
		auto queries = http::uri::split_query(message.relative_uri().query());
		auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
		json::value body = message.extract_json().get();

		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("on")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::PUT, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::POWERON, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("param")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::POST, message);

				if (queries.find(U("Mode")) != queries.end())
					body[U("Mode")] = json::value::string(queries.at(U("Mode")));

				if (queries.find(U("TempHighLimit")) != queries.end())
					body[U("TempHighLimit")] = json::value::number(_wtoi64(queries.at(U("TempHighLimit")).c_str()));

				if (queries.find(U("TempLowLimit")) != queries.end())
					body[U("TempLowLimit")] = json::value::number(_wtoi64(queries.at(U("TempLowLimit")).c_str()));

				if (queries.find(U("DefaultTargetTemp")) != queries.end())
					body[U("DefaultTargetTemp")] = json::value::number(_wtoi64(queries.at(U("DefaultTargetTemp")).c_str()));

				if (queries.find(U("FeeRateH")) != queries.end())
					body[U("FeeRateH")] = json::value::number(_wtof(queries.at(U("FeeRateH")).c_str()));

				if (queries.find(U("FeeRateM")) != queries.end())
					body[U("FeeRateM")] = json::value::number(_wtof(queries.at(U("FeeRateM")).c_str()));

				if (queries.find(U("FeeRateL")) != queries.end())
					body[U("FeeRateL")] = json::value::number(_wtof(queries.at(U("FeeRateL")).c_str()));

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::SETPARAM, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::GET, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::STARTUP, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("state")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				int64_t handler = _fetch(U("Admin"), method_t::GET, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::MONITOR, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("off")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::DEL, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::SHUTDOWN, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("ac")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				int64_t handler = _fetch(paths[2], method_t::GET, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::FETCHFEE, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("detail")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				if (queries.find(U("DateIn")) != queries.end())
					body[U("DateIn")] = json::value::number(_wtoi64(queries.at(U("DateIn")).c_str()));
				if (queries.find(U("DateOut")) != queries.end())
					body[U("DateOut")] = json::value::number(_wtoi64(queries.at(U("DateOut")).c_str()));

				int64_t handler = _fetch(U("Reception"), method_t::GET, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::FETCHINVOICE, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("bill")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				if (queries.find(U("DateIn")) != queries.end())
					body[U("DateIn")] = json::value::number(_wtoi64(queries.at(U("DateIn")).c_str()));
				if (queries.find(U("DateOut")) != queries.end())
					body[U("DateOut")] = json::value::number(_wtoi64(queries.at(U("DateOut")).c_str()));

				int64_t handler = _fetch(U("Reception"), method_t::GET, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::FETCHBILL, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("report")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				int64_t type = 0;
				std::swscanf(paths[3].c_str(), U("%I64d"), &type);
				body[U("TypeReport")] = json::value::number(type);

				int64_t datein = 0;
				std::swscanf(paths[4].c_str(), U("%I64d"), &datein);
				body[U("Date")] = json::value::number(datein);

				int64_t handler = _fetch(U("Manager"), method_t::GET, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::FETCHREPORT, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else
			{
				// message.reply(status_codes::BadRequest);
			}
		}
		else
		{
			// message.reply(status_codes::BadRequest);
		}
	}
	catch (...)
	{
		message.reply(status_codes::BadRequest);
		_listener->support(
			methods::GET,
			std::bind(&ACCom::_handle_get, this, std::placeholders::_1)
		);
	}

	return;
}

void ACCom::_handle_put(http_request message)
{
	try
	{
		auto queries = http::uri::split_query(message.relative_uri().query());
		auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
		json::value body = message.extract_json().get();

		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("ac")) == 0)
			{
				if (paths[2].compare(U("notify")) == 0)
				{
					int64_t rid = 0;
					std::swscanf(paths[3].c_str(), U("%I64d"), &rid);
					body[U("RoomId")] = json::value::number(rid);

					if (queries.find(U("CurrentRoomTemp")) != queries.end())
						body[U("CurrentRoomTemp")] = json::value::number(_wtof(queries.at(U("CurrentRoomTemp")).c_str()));

					int64_t handler = _fetch(paths[3], method_t::PUT, message);

					_pulllocker.lock();
					_pulls.push(ACMessage{ handler, ACMsgType::TEMPNOTIFICATION, body });
					ReleaseSemaphore(_pullsemophare, 1, NULL);
					_pulllocker.unlock();
				}
				else
				{
					int64_t rid = 0;
					std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
					body[U("RoomId")] = json::value::number(rid);

					int64_t handler = _fetch(paths[2], method_t::PUT, message);

					if (queries.find(U("CurrentRoomTemp")) != queries.end())
						body[U("CurrentRoomTemp")] = json::value::number(_wtof(queries.at(U("CurrentRoomTemp")).c_str()));

					_pulllocker.lock();
					_pulls.push(ACMessage{ handler, ACMsgType::REQUESTON, body });
					ReleaseSemaphore(_pullsemophare, 1, NULL);
					_pulllocker.unlock();
				}
			}
			else if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::PUT, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::POWERON, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			{
				// message.reply(status_codes::BadRequest);
			}
		}
		else
		{
			// message.reply(status_codes::BadRequest);
		}
	}
	catch (...)
	{
		message.reply(status_codes::BadRequest);
		_listener->support(
			methods::PUT,
			std::bind(&ACCom::_handle_put, this, std::placeholders::_1)
		);
	}
	return;
}

void ACCom::_handle_post(http_request message)
{
	try
	{
		auto queries = http::uri::split_query(message.relative_uri().query());
		auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
		json::value body = message.extract_json().get();

		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::POST, message);

				if (queries.find(U("Mode")) != queries.end())
					body[U("Mode")] = json::value::string(queries.at(U("Mode")));

				if (queries.find(U("TempHighLimit")) != queries.end())
					body[U("TempHighLimit")] = json::value::number(_wtoi64(queries.at(U("TempHighLimit")).c_str()));
				
				if (queries.find(U("TempLowLimit")) != queries.end())
					body[U("TempLowLimit")] = json::value::number(_wtoi64(queries.at(U("TempLowLimit")).c_str()));
				
				if (queries.find(U("DefaultTargetTemp")) != queries.end())
					body[U("DefaultTargetTemp")] = json::value::number(_wtoi64(queries.at(U("DefaultTargetTemp")).c_str()));
				
				if (queries.find(U("FeeRateH")) != queries.end())
					body[U("FeeRateH")] = json::value::number(_wtof(queries.at(U("FeeRateH")).c_str()));

				if (queries.find(U("FeeRateM")) != queries.end())
					body[U("FeeRateM")] = json::value::number(_wtof(queries.at(U("FeeRateM")).c_str()));

				if (queries.find(U("FeeRateL")) != queries.end())
					body[U("FeeRateL")] = json::value::number(_wtof(queries.at(U("FeeRateL")).c_str()));

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::SETPARAM, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("ac")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				if (queries.find(U("TargetTemp")) != queries.end())
					body[U("TargetTemp")] = json::value::number(_wtoi64(queries.at(U("TargetTemp")).c_str()));
				else if (queries.find(U("FanSpeed")) != queries.end())
					body[U("FanSpeed")] = json::value::number(_wtof(queries.at(U("FanSpeed")).c_str()));

				int64_t handler = _fetch(paths[2], method_t::POST, message);
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
				{
					// message.reply(status_codes::BadRequest);
				}
			}
			else
			{
				// message.reply(status_codes::BadRequest);
			}
		}
		else
		{
			// message.reply(status_codes::BadRequest);
		}
	}
	catch (...)
	{
		message.reply(status_codes::BadRequest);
		_listener->support(
			methods::POST,
			std::bind(&ACCom::_handle_post, this, std::placeholders::_1)
		);
	}
	return;
}

void ACCom::_handle_delete(http_request message)
{
	try
	{
		auto queries = http::uri::split_query(message.relative_uri().query());
		auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
		json::value body = message.extract_json().get();

		if (paths[0].compare(U("api")) == 0)
		{
			if (paths[1].compare(U("power")) == 0)
			{
				int64_t handler = _fetch(U("Admin"), method_t::DEL, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::SHUTDOWN, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else if (paths[1].compare(U("ac")) == 0)
			{
				int64_t rid = 0;
				std::swscanf(paths[2].c_str(), U("%I64d"), &rid);
				body[U("RoomId")] = json::value::number(rid);

				int64_t handler = _fetch(paths[2], method_t::DEL, message);

				_pulllocker.lock();
				_pulls.push(ACMessage{ handler, ACMsgType::REQUESTOFF, body });
				ReleaseSemaphore(_pullsemophare, 1, NULL);
				_pulllocker.unlock();
			}
			else
			{
				// message.reply(status_codes::BadRequest);
			}
		}
		else
		{
			// message.reply(status_codes::BadRequest);
		}
	}
	catch (...)
	{
		message.reply(status_codes::BadRequest);
		_listener->support(
			methods::DEL,
			std::bind(&ACCom::_handle_delete, this, std::placeholders::_1)
		);
	}
	return;
}

void ACCom::_handle_options(http_request message)
{
	auto queries = http::uri::split_query(message.relative_uri().query());
	auto paths = http::uri::split_path(http::uri::decode(message.relative_uri().path()));
	json::value body = message.extract_json().get();

	if (std::find(paths.begin(), paths.end(), U("api")) != paths.end()
		&& std::find(paths.begin(), paths.end(), U("param")) != paths.end())
	{
		int64_t handler = _fetch(U("Admin"), method_t::POST, message);

		if (queries.find(U("Mode")) != queries.end())
			body[U("Mode")] = json::value::string(queries.at(U("Mode")));

		if (queries.find(U("TempHighLimit")) != queries.end())
			body[U("TempHighLimit")] = json::value::number(_wtoi64(queries.at(U("TempHighLimit")).c_str()));

		if (queries.find(U("TempLowLimit")) != queries.end())
			body[U("TempLowLimit")] = json::value::number(_wtoi64(queries.at(U("TempLowLimit")).c_str()));

		if (queries.find(U("DefaultTargetTemp")) != queries.end())
			body[U("DefaultTargetTemp")] = json::value::number(_wtoi64(queries.at(U("DefaultTargetTemp")).c_str()));

		if (queries.find(U("FeeRateH")) != queries.end())
			body[U("FeeRateH")] = json::value::number(_wtof(queries.at(U("FeeRateH")).c_str()));

		if (queries.find(U("FeeRateM")) != queries.end())
			body[U("FeeRateM")] = json::value::number(_wtof(queries.at(U("FeeRateM")).c_str()));

		if (queries.find(U("FeeRateL")) != queries.end())
			body[U("FeeRateL")] = json::value::number(_wtof(queries.at(U("FeeRateL")).c_str()));

		_pulllocker.lock();
		_pulls.push(ACMessage{ handler, ACMsgType::SETPARAM, body });
		ReleaseSemaphore(_pullsemophare, 1, NULL);
		_pulllocker.unlock();
	}
	else
	{
		http_response rep;
		rep.headers().add(U("Content-Type"), U("application/json"));
		rep.headers().add(U("Access-Control-Allow-Origin"), U("*"));
		rep.headers().add(U("Access-Control-Request-Method"), U("GET,POST,PUT,DELETE"));
		rep.headers().add(U("Access-Control-Allow-Credentials"), U("true"));
		rep.headers().add(U("Access-Control-Max-Age"), U("1800"));
		rep.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type,Access-Token,x-requested-with,Authorization"));
		rep.set_status_code(status_codes::OK);
		message.reply(rep)
			//rep.reply(status_codes::OK, out.body)
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
}

void ACCom::_reply()
{
	ACMessage out;
	while (_running)
	{
		out.type = ACMsgType::INVALID;
		WaitForSingleObject(_pushsemophare, 2000);
		
		_pushlocker.lock();
		if (!_pushs.empty())
		{
			out = _pushs.front();
			_pushs.pop();
		}
		_pushlocker.unlock();

		if (out.type != ACMsgType::INVALID)
		{
			//try
			//{
				LPToken token = (LPToken)conv(out.token);
				_tlocker.lock();
				auto recver = std::find_if(_tokens.begin(), _tokens.end(),
					[&token](std::pair<LPToken, std::map<method_t, std::list<http_request>>>& cur) {
					return cur.first == token;
				});
				if (recver != _tokens.end())
				{
					method_t method;
					switch (out.type)
					{
					case ACMsgType::REQUESTON:
					case ACMsgType::POWERON:
					case ACMsgType::TEMPNOTIFICATION:
						method = method_t::PUT;
						break;

					case ACMsgType::SETTEMP:
					case ACMsgType::SETFANSPEED:
					case ACMsgType::SETPARAM:
						method = method_t::POST;
						break;

					case ACMsgType::REQUESTOFF:
					case ACMsgType::SHUTDOWN:
						method = method_t::DEL;
						break;

					case ACMsgType::STARTUP:
					case ACMsgType::FETCHFEE:
					case ACMsgType::FETCHINVOICE:
					case ACMsgType::FETCHREPORT:
					case ACMsgType::FETCHBILL:
					case ACMsgType::MONITOR:
						method = method_t::GET;
						break;

					default:
						break;
					}

					if (((*recver).second).find(method) != (*recver).second.end())
					{
						if (((*recver).second)[method].size() > 0)
						{
							http_request rep = std::move(((*recver).second)[method].front());
							((*recver).second)[method].pop_front();
							_tlocker.unlock();

							http_response msg;
							msg.headers().add(U("Access-Control-Allow-Origin"), U("*"));
							msg.headers().add(U("Access-Control-Request-Method"), U("GET,POST,OPTIONS"));
							msg.headers().add(U("Access-Control-Allow-Credentials"), U("true"));
							msg.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type,Access-Token,x-requested-with,Authorization"));
							msg.set_status_code(status_codes::OK);
							msg.set_body(out.body);
							rep.reply(msg);
						}
						else
						{
							_tlocker.unlock();
						}
					}
					else
					{
						_tlocker.unlock();
					}
				}
				else
				{
					_tlocker.unlock();
				}
			//}
			//catch (...)
			//{
			//	_replycontroller = std::move(std::thread{ std::bind(&ACCom::_reply, this) });
			//}
		}
	}
}
