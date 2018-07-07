#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisSubscriber

(C) 2016 n.lee
*/
#include "base/IRedisService.h"

#include "io/RedisTrunkQueue.h"
#include "io/KjRedisSubscriberWorkQueue.hpp"

//------------------------------------------------------------------------------
/**
@brief CRedisSubscriber
*/
class CRedisSubscriber : public IRedisSubscriber {
public:
	explicit CRedisSubscriber(kj::Own<KjSimpleIoContext> rootContext, redis_stub_param_t& param);
	virtual ~CRedisSubscriber();

	struct channel_message_callback_holder_t {
		std::string _name;
		channel_message_cb_t _subscribe_cb;
	};

	struct pattern_message_callback_holder_t {
		std::string _name;
		pattern_message_cb_t _subscribe_cb;
	};

public:
	virtual void				AddChannelMessageCb(const std::string& sName, const channel_message_cb_t& cb) override;
	virtual void				RemoveChannelMessageCb(const std::string& sName) override;

	virtual void				AddPatternMessageCb(const std::string& sName, const pattern_message_cb_t& cb) override;
	virtual void				RemovePatternMessageCb(const std::string& sName) override;

	virtual void				RunOnce() override {
		_trunkQueue->RunOnce();
	}

	virtual void				Subscribe(const std::string& channel) override;
	virtual void				Psubscribe(const std::string& pattern) override;
	virtual void				Unsubscribe(const std::string& channel) override;
	virtual void				Punsubscribe(const std::string& pattern) override;

	virtual void				Publish(const std::string& channel, std::string& message) override;
	virtual void				Pubsub(std::string& subcommand, std::vector<std::string>& vArg, RESULT_PAIR_LIST& vOut) override;

	virtual void				Shutdown() override;

private:
	void						Send(const std::vector<std::string>& vPiece);

	void						CommitChannel(const std::string& channel);
	void						CommitPattern(const std::string& pattern);
	void						Commit(redis_reply_cb_t&& reply_cb);

	CRedisReply					BlockingCommit();

private:
	CRedisTrunkQueuePtr _trunkQueue;
	CKjRedisSubscriberWorkQueuePtr _workQueue;

	std::vector<channel_message_callback_holder_t> _vChanMsgCbHolder;
	std::vector<pattern_message_callback_holder_t> _vPatMsgCbHolder;

	std::map<std::string, int> _mapChannelSubscriberCounter;
	std::map<std::string, int> _mapPatternSubscriberCounter;

	std::function<void(std::string&, std::string&)> _workCb1;
	std::function<void(std::string&, std::string&, std::string&)> _workCb2;

	std::string _singleCommand;
	std::string _allCommands;
	int _builtNum = 0;
	int _nextSn = 0;
};

/*EOF*/