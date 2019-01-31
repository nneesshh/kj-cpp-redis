//------------------------------------------------------------------------------
//  RedisClient.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisClient.h"

#include <future>
#include "RedisRootContextDef.hpp"

#include "base/RedisError.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

#define SINGLE_COMMAND_RESERVE_SIZE  1024 * 4
#define ALL_COMMANDS_RESERVE_SIZE    1024 * 256

//------------------------------------------------------------------------------
/**

*/
CRedisClient::CRedisClient(redis_stub_param_t& param)
	: _refParam(param) {
	//
	_workQueue = std::make_shared<CKjRedisClientWorkQueue>(this, param);
	_trunkQueue = std::make_shared<CRedisClientTrunkQueue>(this);

	//
	_singleCommand.reserve(SINGLE_COMMAND_RESERVE_SIZE);
	_allCommands.reserve(ALL_COMMANDS_RESERVE_SIZE);

	//
	StartPipeWorker();
}

//------------------------------------------------------------------------------
/**

*/
CRedisClient::~CRedisClient() noexcept {
	_refPipeWorker = nullptr;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisClient::Commit(redis_reply_cb_t&& rcb) {

	auto workCb = std::bind([this](redis_reply_cb_t& reply_cb, CRedisReply& reply) {
		if (reply_cb)
			_trunkQueue->Add(std::move(reply_cb), std::move(reply));
	}, std::move(rcb), std::move(std::placeholders::_1));

	auto cp = CKjRedisClientWorkQueue::CreateCmdPipeline(
		++_nextSn,
		_allCommands,
		_builtNum,
		std::move(workCb),
		nullptr);

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisClient::Commit()] Nothing to commit!!!");
	}
#endif

	_workQueue->Add(std::move(cp));
	_allCommands.resize(0);
	_builtNum = 0;
}

//------------------------------------------------------------------------------
/**

*/
CRedisReply
CRedisClient::BlockingCommit() {

	CRedisReply reply;
	auto workCb = [this, &reply](CRedisReply&& r) {
		reply = std::move(r);
	};

	auto prms = std::make_shared<std::promise<void>>();
	auto disposeCb = [&prms]() {
		prms->set_value();
	};

	auto cp = CKjRedisClientWorkQueue::CreateCmdPipeline(
		++_nextSn,
		_allCommands,
		_builtNum,
		std::move(workCb),
		std::move(disposeCb));

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisClient::BlockingCommit()] Nothing to commit!!!");
	}
#endif

	_workQueue->Add(std::move(cp));
	_allCommands.resize(0);
	_builtNum = 0;

	prms->get_future().get();
	return reply;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisClient::Shutdown() {
	_workQueue->Finish();
	_trunkQueue->Close();
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisClient::StartPipeWorker() {
	// create pipe thread
	_refPipeWorker = redis_get_servercore()->NewPipeWorker(
		"redis client pipeworker",
		_trunkOpCodeRecvBuf,
		sizeof(_trunkOpCodeRecvBuf),
		[this](size_t amount) { _trunkQueue->RunOnce();	},
		[this](svrcore_pipeworker_t *worker) {
		// work
		_workQueue->Run(worker);
	});
}

/** -- EOF -- **/