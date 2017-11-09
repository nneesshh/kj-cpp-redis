//------------------------------------------------------------------------------
//  RedisService.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"

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
CRedisService::CRedisService(redis_stub_param_t *param)
	: _param(*param) {
	//
	_singleCommand.reserve(SINGLE_COMMAND_RESERVE_SIZE);
	_allCommands.reserve(ALL_COMMANDS_RESERVE_SIZE);

	// only one thread
	int nProducerNum = 1;
	_trunkQueue = std::make_shared<CRedisTrunkQueue>();
	_workQueue = std::make_shared<CKjRedisWorkQueue>(_param);
}

//------------------------------------------------------------------------------
/**

*/
CRedisService::~CRedisService() {
	_workQueue->Finish();
	_trunkQueue->Close();
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
CRedisService::Commit(const reply_cb_t& cb) {
	reply_cb_t func = std::bind([this](reply_cb_t& reply_cb, CRedisReply& reply) {
		_trunkQueue->Add(std::move(reply_cb), std::move(reply));
	}, std::move(cb), std::placeholders::_1);

	cmd_pipepline_t cp;

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisService::Commit()] Nothing to commit!!!");
	}
#endif

	cp._sn = ++_nextSn;
	cp._commands.append(_allCommands);
	cp._built_num = _builtNum;
	cp._processed_num = 0;
	cp._reply_cb = std::move(func);
	cp._dispose_cb = nullptr;
	cp._state = cmd_pipepline_t::CMD_PIPELINE_STATE_QUEUEING;

	_workQueue->Add(std::move(cp));
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

	cmd_pipepline_t cp;

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisService::BlockingCommit()] Nothing to commit!!!");
	}
#endif

	cp._sn = ++_nextSn;
	cp._commands.append(_allCommands);
	cp._built_num = _builtNum;
	cp._processed_num = 0;
	cp._reply_cb = std::move(func);
	cp._dispose_cb = std::move(dispose_cb);
	cp._state = cmd_pipepline_t::CMD_PIPELINE_STATE_QUEUEING;

	_workQueue->Add(std::move(cp));
	_allCommands.resize(0);
	_builtNum = 0;

	prms->get_future().get();
	return reply;
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

	CRedisReply&& reply = BlockingCommit();
	if (reply.is_error())
		goto LABEL_LOOT_START;
	return reply;
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