#pragma once
//------------------------------------------------------------------------------
/**
@class IRedisService

(C) 2016 n.lee
*/
#include <map>
#include <vector>
#include <string>
#include <functional>

#include "platform_types.h"
#include "RedisReply.h"

class IRedisClient;
class IRedisSubscriber;

//------------------------------------------------------------------------------
/**
@brief IRedisService
*/
class MY_REDIS_EXTERN IRedisService {
public:
	virtual ~IRedisService() {};

	virtual void				RunOnce() = 0;

	virtual IRedisClient&		Client() = 0;
	virtual IRedisSubscriber&	Subscriber() = 0;

	virtual void				Release() = 0;

};

class IRedisClient {
public:
	virtual ~IRedisClient() {};

	virtual void				RunOnce() = 0;

	virtual void				Send(const std::vector<std::string>& vInputPiece) = 0;
	virtual void				Commit(redis_reply_cb_t&& reply_cb) = 0;
	virtual CRedisReply			BlockingCommit() = 0;

	virtual void				Watch(const std::string& key) = 0;
	virtual void				Multi() = 0;
	virtual void				Exec() = 0;

	virtual void				Set(const std::string& key, std::string& val) = 0;
	virtual void				Get(const std::string& key) = 0;
	virtual void				GetSet(const std::string& key, std::string& val) = 0;
	virtual void				Del(std::vector<std::string>& vKey) = 0;

	virtual void				LPush(const std::string& key, std::vector<std::string>& vVal) = 0;
	virtual void				RPush(const std::string& key, std::vector<std::string>& vVal) = 0;
	virtual void				LPop(const std::string& key) = 0;
	virtual void				RPop(const std::string& key) = 0;
	virtual void				LLen(const std::string& key) = 0;
	virtual void				LIndex(const std::string& key, int nIndex) = 0;
	virtual void				LRange(const std::string& key, int nStart, int nStop) = 0;
	virtual void				LTrim(const std::string& key, int nStart, int nStop) = 0;

	virtual void				HDel(const std::string& key, std::vector<std::string>& vField) = 0;
	virtual void				HGetAll(const std::string& key) = 0;
	virtual void				HGet(const std::string& key, const std::string& field) = 0;
	virtual void				HSet(const std::string& key, const std::string& field, std::string& val) = 0;

	virtual void				ZAdd(const std::string& key, std::string& score, std::string& member) = 0;
	virtual void				ZRem(const std::string& key, std::vector<std::string>& vMember) = 0;
	virtual void				ZScore(const std::string& key, std::string& member) = 0;
	virtual void				ZRange(const std::string& key, std::string& start, std::string& stop, bool bWithScores = false) = 0;
	virtual void				ZRank(const std::string& key, std::string& member) = 0;

	virtual void				SAdd(const std::string& key, std::vector<std::string>& vMember) = 0;
	virtual void				SMembers(const std::string& key) = 0;

	virtual void				ScriptLoad(const std::string& script) = 0;
	virtual void				Eval(const std::string& script, std::vector<std::string>& vKey, std::vector<std::string>& vArg) = 0;
	virtual void				EvalSha(const std::string& sha, std::vector<std::string>& vKey, std::vector<std::string>& vArg) = 0;

};

class IRedisSubscriber {
public:
	virtual ~IRedisSubscriber() {};

	using channel_message_cb_t = std::function<void(const std::string& chan, const std::string& msg)>;
	using pattern_message_cb_t = std::function<void(const std::string& pat, const std::string& chan, const std::string& msg)>;

	using RESULT_PAIR_LIST = std::vector<CRedisReply>;

	virtual void				AddChannelMessageCb(const std::string& sName, const channel_message_cb_t& cb) = 0;
	virtual void				RemoveChannelMessageCb(const std::string& sName) = 0;

	virtual void				AddPatternMessageCb(const std::string& sName, const pattern_message_cb_t& cb) = 0;
	virtual void				RemovePatternMessageCb(const std::string& sName) = 0;

	virtual void				RunOnce() = 0;

	virtual void				Subscribe(const std::string& channel) = 0;
	virtual void				Psubscribe(const std::string& pattern) = 0;
	virtual void				Unsubscribe(const std::string& channel) = 0;
	virtual void				Punsubscribe(const std::string& pattern) = 0;

	virtual void				Publish(const std::string& channel, std::string& message) = 0;
	virtual void				Pubsub(std::string& subcommand, std::vector<std::string>& vArg, RESULT_PAIR_LIST& vOut) = 0;

};

/*EOF*/