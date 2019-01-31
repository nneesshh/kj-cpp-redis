//------------------------------------------------------------------------------
//  KjRedisClientConn.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjRedisClientConn.hpp"

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID

#include "servercore/capnp/kj/debug.h"

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

#include <time.h>

#include "../RedisRootContextDef.hpp"
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
	fprintf(ferr, "redis client crashed:\n%s\n",
		err_txt);
	fclose(ferr);
}

static uint64_t s_redis_client_connid = 91110000;

//------------------------------------------------------------------------------
/**

*/
KjRedisClientConn::KjRedisClientConn(kj::Own<KjPipeEndpointIoContext> endpointContext, redis_stub_param_t& param)
	: _endpointContext(kj::mv(endpointContext))
	, _refParam(param)
	, _tsCommon(redis_get_servercore()->NewTaskSet(*this))
	, _kjconn(kj::addRef(*_endpointContext), ++s_redis_client_connid) {
	//
	Init();
}

//------------------------------------------------------------------------------
/**
*/
KjRedisClientConn::~KjRedisClientConn() {
	Disconnect();
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::OnClientConnect(KjRedisTcpConn&, uint64_t connid) {
	// "redis client start read op"
	auto p1 = _kjconn.StartReadOp(
		std::bind(&KjRedisClientConn::OnClientReceive,
			this,
			std::placeholders::_1,
			std::placeholders::_2)
	);

	// schedule
	redis_get_servercore()->ScheduleTask(*_tsCommon.get(), kj::mv(p1));

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
KjRedisClientConn::OnClientDisconnect(KjRedisTcpConn&, uint64_t connid) {

}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::OnClientReceive(KjRedisTcpConn&, bip_buf_t& bb) {

	try {
		_builder.ProcessInput(bb);
	}
	catch (const CRedisError&) {
		return;
	}

	while (_builder.IsReplyAvailable()) {

		CRedisReply reply = _builder.PopReply();
		if (reply.is_error()) {

			std::string sDesc = "[KjRedisClientConn::OnClientReceive()] !!! reply error !!! ";
			sDesc += reply.as_string();
			fprintf(stderr, "\n\n\n%s\n", sDesc.c_str());
			throw CRedisError(sDesc.c_str());
		}

		auto& cp = _dqCommon.front();

		// check committing num
		if (_committing_num <= 0
			|| _dqCommon.size() <= 0) {

			std::string sDesc = "[KjRedisClientConn::OnClientReceive()] !!! got reply but the cmd pipeline is already discarded !!! ";
			fprintf(stderr, "\n\n\n%s\n", sDesc.c_str());
			throw CRedisError(sDesc.c_str());
		}

		// check cmd pipeline state
		if (cp._state < redis_cmd_pipepline_t::COMMITTING) {

			std::string sDesc = "[KjRedisClientConn::OnClientReceive()] !!! got reply but the cmd pipeline is in a corrupted state !!! ";
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

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::Open(redis_stub_param_t& param) {

	fprintf(stderr, "[KjRedisClientConn::Open()] connect to ip(%s)port(%d)...\n",
		_refParam._ip.c_str(), _refParam._port);

	Connect(param._ip, param._port);
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::Close() {
	
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::Connect(const std::string& host, unsigned short port) {
	// "redis client connect"
	auto p1 = _kjconn.Connect(
		host,
		port,
		std::bind(&KjRedisClientConn::OnClientConnect, this, std::placeholders::_1, std::placeholders::_2),
		std::bind(&KjRedisClientConn::OnClientDisconnect, this, std::placeholders::_1, std::placeholders::_2)
	);

	// schedule
	redis_get_servercore()->ScheduleTask(*_tsCommon.get(), kj::mv(p1));
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::Disconnect() {
	_kjconn.Disconnect();
}

//------------------------------------------------------------------------------
/**

*/
KjRedisClientConn&
KjRedisClientConn::Commit() {

	if (_dqCommon.size() > 0) {

		// stream write maybe trigger exception at once, so we must catch it manually
		KJ_IF_MAYBE(e, kj::runCatchingExceptions(
			[this]() {
			// schedule -- "commit loop"
			redis_get_servercore()->ScheduleTask(*_tsCommon.get(), CommitLoop());
		})) {
			taskFailed(kj::mv(*e));
		};
	}
	return *this;
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::Init() {
	//
	std::string sCommands;
	int nBuiltNum = 0;

	// auth
	const std::string& sPassword = _refParam._sPassword;
	if (sPassword.length() > 0) {

		std::string sSingleCommand;
		CRedisCommandBuilder::Build({ "AUTH", sPassword }, sSingleCommand, sCommands, nBuiltNum);

		//
		auto cp = CKjRedisClientWorkQueue::CreateCmdPipeline(
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

	// test cmsgpack
	std::string sTestcmsgpack("577017df0566f25df93daa49115c0290597c3c36");
	_refParam._mapScript[sTestcmsgpack] = "local t={1,'abc2',3};local p=cmsgpack.pack(t);local u=cmsgpack.unpack(p);return {t,p,u}";

	// register script
	for (auto& iter : _refParam._mapScript) {

		const std::string& sSha = iter.first;
		const std::string& sScript = iter.second;

		// script load
		std::string sSingleCommand;
		CRedisCommandBuilder::Build({ "SCRIPT", "LOAD", sScript }, sSingleCommand, sCommands, nBuiltNum);

		//
		redis_reply_cb_t func = [sSha, sScript](CRedisReply&& reply) {
			CRedisReply r = std::move(reply);
			bool bRegisterSuccess = false;
			std::string sExpected;
			if (r.ok()
				&& r.is_string()) {
				sExpected = r.as_string();
				if (sSha == sExpected) {
					bRegisterSuccess = true;
				}
			}

			if (!bRegisterSuccess) {

				std::string sDesc = "[KjRedisClientConn::Init()] !!! register script error !!! ";
				sDesc += "\nscript=\"" + sScript + "\"\nsha='" + sSha + "\"\nexpected=\"" + sExpected + "\"\n";
				fprintf(stderr, "\n\n\n%s\n", sDesc.c_str());
				throw CRedisError(sDesc.c_str());
			}
		};

		auto cp = CKjRedisClientWorkQueue::CreateCmdPipeline(
			0,
			sCommands,
			nBuiltNum,
			std::move(func),
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
KjRedisClientConn::DelayReconnect() {
	//
	const int nDelaySeconds = 3;

	if (!_bDelayReconnecting) {
		_bDelayReconnecting = true;

		fprintf(stderr, "[KjRedisClientConn::DelayReconnect()] connid(%08llu)host(%s)port(%d), reconnect after (%d) seconds...\n",
			_kjconn.GetConnId(), _kjconn.GetHost().cStr(), _kjconn.GetPort(), nDelaySeconds);

		// "redis client delay reconnect"
		auto p1 = _endpointContext->AfterDelay(nDelaySeconds * kj::SECONDS)
			.then([this]() {

			_bDelayReconnecting = false;
			return _kjconn.Reconnect();
		});

		// schedule
		redis_get_servercore()->ScheduleTask(*_tsCommon.get(), kj::mv(p1));
	}
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisClientConn::CommitLoop() {

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
		// "redis client commit loop"
		auto p1 = _endpointContext->AfterDelay(1 * kj::SECONDS)
			.then([this]() {

			return CommitLoop();
		});
		return p1;
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisClientConn::taskFailed(kj::Exception&& exception) {
	char chDesc[1024];
#if defined(__CYGWIN__) || defined( _WIN32)
	_snprintf_s(chDesc, sizeof(chDesc), "\n[KjRedisClientConn::taskFailed()] desc(%s) -- auto reconnect.\n",
		exception.getDescription().cStr());
#else
	snprintf(chDesc, sizeof(chDesc), "\n[KjRedisClientConn::taskFailed()] desc(%s) -- auto reconnect.\n",
		exception.getDescription().cStr());
#endif
	fprintf(stderr, chDesc);
	write_redis_connection_crash_error(chDesc);

	// schedule eval later to avoid destroying self task set
	redis_get_servercore()->ScheduleEvalLaterFunc([this]() {

		// flush stream
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
}

/** -- EOF -- **/