//------------------------------------------------------------------------------
//  KjRedisSubscriberConn.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjRedisSubscriberConn.hpp"

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID

#include <kj/debug.h>

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

#include <time.h>

#include "base/RedisError.h"
#include "RedisService.h"

#include "RedisCommandBuilder.h"

static void
write_redis_connection_crash_error(const char *err_txt) {
	char log_file_name[256];
	char time_suffix[32];

	time_t t = time(nullptr);

#if defined(__CYGWIN__) || defined( _WIN32)
	struct tm tp;
	localtime_s(&tp, &t);
	_snprintf_s(
		time_suffix
		, sizeof(time_suffix)
		, "_%d-%02d-%02d_%02d_%02d_%02d"
		, tp.tm_year + 1900
		, tp.tm_mon + 1
		, tp.tm_mday
		, tp.tm_hour
		, tp.tm_min
		, tp.tm_sec);
#else
	struct tm tp;
	localtime_r(&t, &tp);
	snprintf(
		time_suffix
		, sizeof(time_suffix)
		, "_%d-%02d-%02d_%02d_%02d_%02d"
		, tp.tm_year + 1900
		, tp.tm_mon + 1
		, tp.tm_mday
		, tp.tm_hour
		, tp.tm_min
		, tp.tm_sec);
#endif

#if defined(__CYGWIN__) || defined( _WIN32)
	_snprintf_s(log_file_name, sizeof(log_file_name), "syslog/redis_connection_crash_%s.error",
		time_suffix);
#else
	snprintf(log_file_name, sizeof(log_file_name), "syslog/redis_connection_crash_%s.error",
		time_suffix);
#endif

	FILE *ferr = fopen(log_file_name, "at+");
	fprintf(ferr, "redis subscriber crashed:\n%s\n",
		err_txt);
	fclose(ferr);
}

static uint64_t s_redis_subscriber_connid = 91120000;

//------------------------------------------------------------------------------
/**

*/
KjRedisSubscriberConn::KjRedisSubscriberConn(
	kj::Own<KjSimpleThreadIoContext> tioContext,
	redis_stub_param_t& param,
	const std::function<void(std::string&, std::string&)>& workCb1,
	const std::function<void(std::string&, std::string&, std::string&)>& workCb2)
	: _tioContext(kj::mv(tioContext))
	, _tasks(_tioContext->CreateTaskSet(*this))
	, _refParam(param)
	, _refWorkCb1(workCb1)
	, _refWorkCb2(workCb2	)
	, _kjconn(kj::addRef(*_tioContext), ++s_redis_subscriber_connid) {
	//
	Init();
}

//------------------------------------------------------------------------------
/**
*/
KjRedisSubscriberConn::~KjRedisSubscriberConn() {
	Disconnect();
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::OnClientConnect(KjTcpConnection&, uint64_t connid) {
	// start read
	auto p1 = _kjconn.StartReadOp(
		std::bind(&KjRedisSubscriberConn::OnClientReceive,
			this,
			std::placeholders::_1,
			std::placeholders::_2)
	);
	_tasks->add(kj::mv(p1), "redis subscriber start read op");

	// connection init
	{
		std::deque<redis_cmd_pipepline_t> dqTmp;
		dqTmp.insert(dqTmp.end(), _dqInit.begin(), _dqInit.end());

		for (auto& cp : _dqCommon) {
			if (cp._sn > 0
				&& cp._state < redis_cmd_pipepline_t::PROCESS_OVER) {
				// it is common cmd pipeline and not process over yest
				dqTmp.emplace_back(std::move(cp));
			}
		}
		_dqCommon.swap(dqTmp);

		for (auto& cp : _dqCommon) {
			// resending
			cp._state = redis_cmd_pipepline_t::SENDING;
		}

		// recommit
		Commit();
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::OnClientDisconnect(KjTcpConnection&, uint64_t connid) {

}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::OnClientReceive(KjTcpConnection&, bip_buf_t& bb) {

	try {
		_builder.ProcessInput(bb);
	}
	catch (const CRedisError&) {
		return;
	}

	while (_builder.IsReplyAvailable()) {

		auto&& reply = _builder.PopReply();
		if (reply.is_error()) {

			std::string sDesc = "[KjRedisSubscriberConn::OnClientReceive()] !!! reply error !!! ";
			sDesc += reply.as_string();
			fprintf(stderr, "\n\n\n%s\n", sDesc.c_str());
			throw CRedisError(sDesc.c_str());
		}
	
		bool bIsMessage = false;
		if (reply.ok()
			&& reply.is_array()) {
			std::vector<CRedisReply> v = std::move(reply.as_array());
			if (v.size() >= 4
				&& v[0].is_string()
				&& v[1].is_string()
				&& v[2].is_string()
				&& v[3].is_string()) {
				//
				std::string sMessageType = std::move(v[0].as_string());
				std::string sPattern = std::move(v[1].as_string());
				std::string sChannel = std::move(v[2].as_string());
				std::string sMessage = std::move(v[3].as_string());
			
				//
				bIsMessage = (sMessageType == "pmessage");
				if (bIsMessage) {
					_refWorkCb2(std::move(sPattern), std::move(sChannel), std::move(sMessage));
				}
			}
			else if (v.size() >= 3
				&& v[0].is_string()
				&& v[1].is_string()
				&& v[2].is_string()) {
				//
				std::string sMessageType = std::move(v[0].as_string());
				std::string sChannel = std::move(v[1].as_string());
				std::string sMessage = std::move(v[2].as_string());

				//
				bIsMessage = (sMessageType == "message");
				if (bIsMessage) {
					_refWorkCb1(std::move(sChannel), std::move(sMessage));
				}
			}
		}

		if (!bIsMessage) {
			auto& cp = _dqCommon.front();

			// check committing num
			if (_committing_num <= 0
				|| _dqCommon.size() <= 0) {

				std::string sDesc = "[KjRedisSubscriberConn::OnClientReceive()] !!! got reply but the cmd pipeline is already discarded !!! ";
				fprintf(stderr, "\n\n\n%s\n", sDesc.c_str());
				throw CRedisError(sDesc.c_str());
			}

			// check cmd pipeline state
			if (cp._state < redis_cmd_pipepline_t::COMMITTING) {

				std::string sDesc = "[KjRedisSubscriberConn::OnClientReceive()] !!! got reply but the cmd pipeline is in a corrupted state !!! ";
				fprintf(stderr, "\n\n\n%s\n", sDesc.c_str());
				throw CRedisError(sDesc.c_str());
			}

			// go on processing -- non-tail reply is skipped, just add process num
			++cp._processed_num;
			cp._state = redis_cmd_pipepline_t::PROCESSING;

			// process over -- it is tail reply(the last reply of the cmd pipeline), run callback on it
			if (cp._processed_num >= cp._built_num) {

				if (cp._reply_cb) cp._reply_cb(std::move(reply));
				if (cp._dispose_cb) cp._dispose_cb();

				cp._state = redis_cmd_pipepline_t::PROCESS_OVER;

				//
				_dqCommon.pop_front();
				--_committing_num;
			}
		}
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::Open() {

	fprintf(stderr, "[KjRedisSubscriberConn::Open()] connect to ip(%s)port(%d)...\n",
		_refParam._ip.c_str(), _refParam._port);

	Connect(_refParam._ip, _refParam._port);
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::Close() {

}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::Connect(const std::string& host, unsigned short port) {

	auto p1 = _kjconn.Connect(
		host,
		port,
		std::bind(&KjRedisSubscriberConn::OnClientConnect, this, std::placeholders::_1, std::placeholders::_2),
		std::bind(&KjRedisSubscriberConn::OnClientDisconnect, this, std::placeholders::_1, std::placeholders::_2)
	);
	_tasks->add(kj::mv(p1), "redis subscriber connect");
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::Disconnect() {
	_kjconn.Disconnect();
}

//------------------------------------------------------------------------------
/**

*/
KjRedisSubscriberConn&
KjRedisSubscriberConn::Commit() {

	if (_dqCommon.size() > 0) {
		// stream write maybe trigger exception at once, so we must catch it manually
		KJ_IF_MAYBE(e, kj::runCatchingExceptions(
			[this]() {
			_tasks->add(CommitLoop(), "commit loop");
		})) {
			taskFailed(kj::mv(*e));
		}
	}
	return *this;
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::Init() {
	//
	std::string sCommands;
	int nBuiltNum = 0;

	// auth
	const std::string& sPassword = _refParam._sPassword;
	if (sPassword.length() > 0) {

		std::string sSingleCommand;
		CRedisCommandBuilder::Build({ "AUTH", sPassword }, sSingleCommand, sCommands, nBuiltNum);

		//
		auto cp = CKjRedisSubscriberWorkQueue::CreateCmdPipeline(
			0,
			sCommands,
			nBuiltNum,
			nullptr,
			nullptr);

		//
		_dqInit.emplace_back(std::move(cp));
		sCommands.resize(0);
		nBuiltNum = 0;
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::DelayReconnect() {
	//
	const int nDelaySeconds = 3;

	if (!_bDelayReconnecting) {
		_bDelayReconnecting = true;

		fprintf(stderr, "[KjRedisClientConn::DelayReconnect()] connid(%08llu)host(%s)port(%d), reconnect after (%d) seconds...\n",
			_kjconn.GetConnId(), _kjconn.GetHost().cStr(), _kjconn.GetPort(), nDelaySeconds);


		auto p1 = _tioContext->AfterDelay(nDelaySeconds * kj::SECONDS, "reconnect")
			.then([this]() {

			_bDelayReconnecting = false;
			return _kjconn.Reconnect();
		});
		_tasks->add(kj::mv(p1), "redis subscriber delay reconnect");
	}
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisSubscriberConn::CommitLoop() {
	if (IsConnected()) {
		for (auto& cp : _dqCommon) {
			if (redis_cmd_pipepline_t::SENDING == cp._state) {

				_kjconn.Write(cp._commands.c_str(), cp._commands.length());

				cp._state = redis_cmd_pipepline_t::COMMITTING;
				++_committing_num;
			}
		}

		// commit over
		return kj::READY_NOW;
	}
	else {
		auto p1 = _tioContext->AfterDelay(1 * kj::SECONDS, "redis subscriber commit loop")
			.then([this]() {

			return CommitLoop();
		});
		return kj::mv(p1);
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisSubscriberConn::taskFailed(kj::Exception&& exception) {
	char chDesc[1024];
#if defined(__CYGWIN__) || defined( _WIN32)
	_snprintf_s(chDesc, sizeof(chDesc), "\n[KjRedisSubscriberConn::taskFailed()] desc(%s) -- auto reconnect.\n",
		exception.getDescription().cStr());
#else
	snprintf(chDesc, sizeof(chDesc), "\n[KjRedisSubscriberConn::taskFailed()] desc(%s) -- auto reconnect.\n",
		exception.getDescription().cStr());
#endif
	fprintf(stderr, chDesc);
	write_redis_connection_crash_error(chDesc);

	// eval later to avoid destroying self task set
	auto p1 = _tioContext->EvalForResult(
		[this]() {

		// force flush stream
		_kjconn.FlushStream();

		//
		_builder.Reset();

		// 
		for (auto& cp : _dqCommon) {
			cp._state = redis_cmd_pipepline_t::QUEUEING;
		}
		_committing_num = 0;

		//
		DelayReconnect();
	});
	_tioContext->AddTask(kj::mv(p1), "redis subscriber conn dispose");
}

/** -- EOF -- **/