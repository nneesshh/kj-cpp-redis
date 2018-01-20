//------------------------------------------------------------------------------
//  RedisClient.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisClient.h"

#include <future>

#include "base/RedisError.h"
#include "RedisService.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

#define SINGLE_COMMAND_RESERVE_SIZE	1024 * 4
#define ALL_COMMANDS_RESERVE_SIZE	1024 * 256

//------------------------------------------------------------------------------
/**

*/
CRedisClient::CRedisClient(kj::Own<KjSimpleIoContext> rootContext, redis_stub_param_t& param) {
	//
	_trunkQueue = std::make_shared<CRedisTrunkQueue>();
	_workQueue = std::make_shared<CKjRedisClientWorkQueue>(kj::addRef(*rootContext), param);

	//
	_singleCommand.reserve(SINGLE_COMMAND_RESERVE_SIZE);
	_allCommands.reserve(ALL_COMMANDS_RESERVE_SIZE);
}

//------------------------------------------------------------------------------
/**

*/
CRedisClient::~CRedisClient() {
	_workQueue->Finish();
	_trunkQueue->Close();
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisClient::Send(const std::vector<std::string>& vPiece) {
	_allCommands.append(CRedisService::BuildCommand(vPiece, _singleCommand));
	++_builtNum;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisClient::Commit(redis_reply_cb_t&& cb) {

	auto workCb = std::bind([this](redis_reply_cb_t& reply_cb, CRedisReply& reply) {
		_trunkQueue->Add(std::move(reply_cb), std::move(reply));
	}, std::move(cb), std::placeholders::_1);

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
	auto workCb = [this, &reply](CRedisReply& r) {
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

/** -- EOF -- **/