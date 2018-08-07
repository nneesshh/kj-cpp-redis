//------------------------------------------------------------------------------
//  RedisSubscriber.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisSubscriber.h"

#include <future>

#include "base/RedisError.h"
#include "RedisService.h"

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
CRedisSubscriber::CRedisSubscriber(kj::Own<KjSimpleIoContext> rootContext, redis_stub_param_t& param) {
	//
	_workCb1 = [this](std::string& chan, std::string& msg) {

		auto workCb = std::bind([this](std::string& chan, std::string& msg) {
			for (auto& it : _vChanMsgCbHolder) {
				auto& holder = it;
				if (holder._subscribe_cb) {
					holder._subscribe_cb(chan, msg);
				}
			}
		}, std::move(chan), std::move(msg));

		//
		_trunkQueue->Add(std::move(workCb));
	};

	//
	_workCb2 = [this](std::string& pat, std::string& chan, std::string& msg) {

		auto workCb = std::bind([this](std::string& pat, std::string& chan, std::string& msg) {
			for (auto& it : _vPatMsgCbHolder) {
				auto& holder = it;
				if (holder._subscribe_cb) {
					holder._subscribe_cb(pat, chan, msg);
				}
			}
		}, std::move(pat), std::move(chan), std::move(msg));

		//
		_trunkQueue->Add(std::move(workCb));
	};

	//
	_trunkQueue = std::make_shared<CRedisTrunkQueue>();
	_workQueue = std::make_shared<CKjRedisSubscriberWorkQueue>(
		kj::addRef(*rootContext),
		param,
		_workCb1,
		_workCb2);

	//
	_singleCommand.reserve(SINGLE_COMMAND_RESERVE_SIZE);
	_allCommands.reserve(ALL_COMMANDS_RESERVE_SIZE);
}

//------------------------------------------------------------------------------
/**

*/
CRedisSubscriber::~CRedisSubscriber() {
	
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::AddChannelMessageCb(const std::string& sName, const channel_message_cb_t& cb) {
	for (auto& it : _vChanMsgCbHolder) {
		if (sName == it._name) {
			// exist, replace cb
			it._subscribe_cb = cb;
			return;
		}
	}

	// new cb
	_vChanMsgCbHolder.resize(_vChanMsgCbHolder.size() + 1);
	channel_message_callback_holder_t& holder = _vChanMsgCbHolder[_vChanMsgCbHolder.size() - 1];
	holder._name = sName;
	holder._subscribe_cb = cb;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::RemoveChannelMessageCb(const std::string& sName) {
	std::vector<channel_message_callback_holder_t>::iterator it = _vChanMsgCbHolder.begin(),
		itEnd = _vChanMsgCbHolder.end();
	while (it != itEnd) {
		if (sName == (*it)._name) {
			// erase
			_vChanMsgCbHolder.erase(it);
			return;
		}
		//
		++it;
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::AddPatternMessageCb(const std::string& sName, const pattern_message_cb_t& cb) {
	for (auto& it : _vPatMsgCbHolder) {
		if (sName == it._name) {
			// exist, replace cb
			it._subscribe_cb = cb;
			return;
		}
	}

	// new cb
	_vPatMsgCbHolder.resize(_vPatMsgCbHolder.size() + 1);
	pattern_message_callback_holder_t& holder = _vPatMsgCbHolder[_vPatMsgCbHolder.size() - 1];
	holder._name = sName;
	holder._subscribe_cb = cb;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::RemovePatternMessageCb(const std::string& sName) {
	std::vector<pattern_message_callback_holder_t>::iterator it = _vPatMsgCbHolder.begin(),
		itEnd = _vPatMsgCbHolder.end();
	while (it != itEnd) {
		if (sName == (*it)._name) {
			// erase
			_vPatMsgCbHolder.erase(it);
			return;
		}
		//
		++it;
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Subscribe(const std::string& channel) {

	int nCount = _mapChannelSubscriberCounter[channel];
	if (0 == nCount) {
		// subscribe
		BuildCommand({ "SUBSCRIBE", channel });
		CommitChannel(channel);
	}

	// increase count
	++nCount;
	_mapChannelSubscriberCounter[channel] = nCount;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Psubscribe(const std::string& pattern) {

	int nCount = _mapPatternSubscriberCounter[pattern];
	if (0 >= nCount) {
		//
		nCount = 0;

		// psubscribe
		BuildCommand({ "PSUBSCRIBE", pattern });
		CommitPattern(pattern);
	}

	// increase count
	++nCount;
	_mapPatternSubscriberCounter[pattern] = nCount;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Unsubscribe(const std::string& channel) {

	int nCount = _mapChannelSubscriberCounter[channel];
	if (nCount > 0) {
		// decrease count
		--nCount;
		_mapChannelSubscriberCounter[channel] = nCount;

		if (0 == nCount) {
			// unsubscribe
			BuildCommand({ "UNSUBSCRIBE", channel });
			
			redis_reply_cb_t defaultCb = [](CRedisReply&) {};
			Commit(std::move(defaultCb));
		}
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Punsubscribe(const std::string& pattern) {

	int nCount = _mapPatternSubscriberCounter[pattern];
	
	// decrease count
	--nCount;
	_mapPatternSubscriberCounter[pattern] = nCount;

	if (0 == nCount) {
		// punsubscribe
		BuildCommand({ "PUNSUBSCRIBE", pattern });

		redis_reply_cb_t defaultCb = [](CRedisReply&) {};
		Commit(std::move(defaultCb));
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Publish(const std::string& channel, std::string& message) {
	BuildCommand({ "PUBLISH", channel, std::move(message) });

	redis_reply_cb_t defaultCb = [](CRedisReply&) {};
	Commit(std::move(defaultCb));
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Pubsub(std::string& subcommand, std::vector<std::string>& vArg, RESULT_PAIR_LIST& vOut) {
	size_t szNumArgs = vArg.size();
	std::vector<std::string> vPiece(2 + szNumArgs);
	vPiece.assign({ "PUBSUB", subcommand });
	vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
	BuildCommand(vPiece);

	CRedisReply reply = BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Shutdown() {
	_workQueue->Finish();
	_trunkQueue->Close();
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::CommitChannel(const std::string& channel) {

	redis_reply_cb_t cb = [this, channel](CRedisReply&& reply) {

		if (reply.ok()
			&& reply.is_array()) {
			std::vector<CRedisReply>& v = reply.as_array();
			if (v.size() >= 3
				&& v[0].is_string()
				&& v[1].is_string()
				&& v[2].is_integer()) {
				//
				std::string& sSubscribe = v[0].as_string();
				std::string& sChannel = v[1].as_string();
				int64_t nNumOfSubscribers = v[2].as_integer();

				assert(sSubscribe == "subscribe");
				assert(sChannel == channel);
				assert(nNumOfSubscribers >= 1);

			}
		}
	};

	auto workCb = std::bind([this](redis_reply_cb_t& reply_cb, CRedisReply& reply) {
		if (reply_cb)
			_trunkQueue->Add(std::move(reply_cb), std::move(reply));
	}, std::move(cb), std::move(std::placeholders::_1));

	auto cp = CKjRedisSubscriberWorkQueue::CreateCmdPipeline(
		++_nextSn,
		_allCommands,
		_builtNum,
		std::move(workCb),
		nullptr);

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisSubscriber::CommitChannel()] Nothing to commit!!!");
	}
#endif

	_workQueue->Add(std::move(cp));
	_allCommands.resize(0);
	_builtNum = 0;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::CommitPattern(const std::string& pattern) {

	redis_reply_cb_t cb = [this, pattern](CRedisReply&& reply) {
		
		if (reply.ok()
			&& reply.is_array()) {
			std::vector<CRedisReply>& v = reply.as_array();
			if (v.size() >= 3
				&& v[0].is_string()
				&& v[1].is_string()
				&& v[2].is_integer()) {
				//
				std::string& sPSubscribe = v[0].as_string();
				std::string& sPattern = v[1].as_string();
				int64_t nNumOfSubscribers = v[2].as_integer();

				assert(sPSubscribe == "psubscribe");
				assert(sPattern == pattern);
				assert(nNumOfSubscribers >= 1);

			}
		}
	};

	auto workCb = std::bind([this](redis_reply_cb_t& reply_cb, CRedisReply& reply) {
		if (reply_cb)
			_trunkQueue->Add(std::move(reply_cb), std::move(reply));
	}, std::move(cb), std::move(std::placeholders::_1));

	auto cp = CKjRedisSubscriberWorkQueue::CreateCmdPipeline(
		++_nextSn,
		_allCommands,
		_builtNum,
		std::move(workCb),
		nullptr);

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisSubscriber::CommitPattern()] Nothing to commit!!!");
	}
#endif

	_workQueue->Add(std::move(cp));
	_allCommands.resize(0);
	_builtNum = 0;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriber::Commit(redis_reply_cb_t&& cb) {

	auto workCb = std::bind([this](redis_reply_cb_t& reply_cb, CRedisReply& reply) {
		if (reply_cb)
			_trunkQueue->Add(std::move(reply_cb), std::move(reply));
	}, std::move(cb), std::move(std::placeholders::_1));

	auto cp = CKjRedisSubscriberWorkQueue::CreateCmdPipeline(
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
CRedisSubscriber::BlockingCommit() {

	CRedisReply reply;
	auto workCb = [this, &reply](CRedisReply&& r) {
		reply = std::move(r);
	};

	auto prms = std::make_shared<std::promise<void>>();
	auto disposeCb = [this, &prms]() {
		prms->set_value();
	};

	auto cp = CKjRedisSubscriberWorkQueue::CreateCmdPipeline(
		++_nextSn,
		_allCommands,
		_builtNum,
		std::move(workCb),
		std::move(disposeCb));

#ifdef _DEBUG
	if (_builtNum <= 0) {
		throw CRedisError("[CRedisSubscriber::BlockingCommit()] Nothing to commit!!!");
	}
#endif

	_workQueue->Add(std::move(cp));
	_allCommands.resize(0);
	_builtNum = 0;

	prms->get_future().get();
	return reply;
}

/** -- EOF -- **/