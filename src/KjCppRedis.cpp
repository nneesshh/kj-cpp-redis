//------------------------------------------------------------------------------
//  RedisService.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"
#include "toolkit/framework/RedisError.h"

#include <future>

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
CRedisService::CRedisService(IAppBase *pApp, redis_stub_param_t *param)
	: _refApp(pApp)
	, _param(*param) {
	//
	_singleCommand.reserve(SINGLE_COMMAND_RESERVE_SIZE);
	_allCommands.reserve(ALL_COMMANDS_RESERVE_SIZE);

	// only one thread
	int nProducerNum = 1;
	_trunkQueue = std::make_shared<CRedisTrunkQueue>(_refApp, nProducerNum);
	_workQueue = std::make_shared<CKjRedisWorkQueue>(_refApp->LogHandler(), _param, nProducerNum);
}

//------------------------------------------------------------------------------
/**

*/
CRedisService::~CRedisService() {

	_workQueue->Finish();
	_trunkQueue->Close();

	//
	google::protobuf::ShutdownProtobufLibrary();
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisService::Send(const std::vector<std::string>& vPiece) {
	_allCommands.append(BuildCommand(vPiece));
	++_builtNum;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisService::Commit(const reply_cb_t& reply_cb) {
	reply_cb_t func = std::bind([this](reply_cb_t& reply_cb, CRedisReply& r) {
		_trunkQueue->Add(reply_cb, std::move(r));
	}, std::move(reply_cb), std::placeholders::_1);

	cmd_t cmd;

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisService::Commit()] Nothing to commit!!!");
	}
#endif

	cmd._sn = ++_nextSn;
	cmd._commands.append(_allCommands);
	cmd._built_num = _builtNum;
	cmd._processed_num = 0;
	cmd._reply_cb = std::move(func);
	cmd._dispose_cb = nullptr;
	cmd._state = cmd_t::REDIS_CMD_STATE_QUEUEING;

	_workQueue->Add(cmd);
	_allCommands.resize(0);
	_builtNum = 0;
}

//------------------------------------------------------------------------------
/**

*/
CRedisReply
CRedisService::BlockingCommit() {
	CRedisReply reply;
	reply_cb_t func = [this, &reply](CRedisReply& r) {
		reply = std::move(r);
	};

	auto prms = std::make_shared<std::promise<void>>();
	auto dispose_cb = std::bind([this, &prms]() {
		prms->set_value();
	});

	cmd_t cmd;

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisService::BlockingCommit()] Nothing to commit!!!");
	}
#endif

	cmd._sn = ++_nextSn;
	cmd._commands.append(_allCommands);
	cmd._built_num = _builtNum;
	cmd._processed_num = 0;
	cmd._reply_cb = std::move(func);
	cmd._dispose_cb = std::move(dispose_cb);
	cmd._state = cmd_t::REDIS_CMD_STATE_QUEUEING;

	_workQueue->Add(cmd);
	_allCommands.resize(0);
	_builtNum = 0;

	prms->get_future().get();
	return std::move(reply);
}

//------------------------------------------------------------------------------
/**

*/
CRedisReply
CRedisService::LootHashTable(const std::string& strKey) {
LABEL_LOOT_START:
	Watch(strKey);
	Multi();
	HGetAll(strKey);
	Del(std::vector<std::string>{strKey});
	Exec();

	CRedisReply r = BlockingCommit();
	if (r.is_error())
		goto LABEL_LOOT_START;
	return std::move(r);
}

//------------------------------------------------------------------------------
/**

*/
CRedisReply
CRedisService::LootHashTableField(const std::string& strKey, const std::string& strField) {
	Multi();
	HGet(strKey, strField);
	HDel(strKey, std::vector<std::string>{ strField });
	Exec();
	return BlockingCommit();
}

/** -- EOF -- **/