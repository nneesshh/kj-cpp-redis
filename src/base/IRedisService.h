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

#ifdef _WIN32
#pragma comment(lib, "WS2_32.Lib")
#pragma comment(lib, "Wldap32.Lib")
#pragma comment(lib, "IPHlpApi.Lib")
#pragma comment(lib, "Psapi.Lib")
#pragma comment(lib, "UserEnv.Lib")
#endif

#include "redis_extern.h"
#include "RedisReply.h"

#ifdef __cplusplus 
extern "C" {
#endif 
#include "rdb_parser/rdb_parser.h"
#ifdef __cplusplus 
}
#endif 

class IRedisClient;
class IRedisSubscriber;

//------------------------------------------------------------------------------
/**
	@brief IRedisService
*/
class MY_REDIS_EXTERN IRedisService {
public:
	virtual ~IRedisService() noexcept {};

	virtual void				OnUpdate() = 0;

	virtual IRedisClient&		Client() = 0;
	virtual IRedisSubscriber&	Subscriber() = 0;

	virtual int					ParseDumpedData(const std::string& sDump, std::function<int(rdb_object_t *)>&& cb) = 0;

	virtual void				Shutdown() = 0;
};

class MY_REDIS_EXTERN IRedisClient {
public:
	virtual ~IRedisClient() noexcept {};

	virtual void				RunOnce() = 0;

	virtual void				Commit(redis_reply_cb_t&& rcb) = 0;
	virtual CRedisReply			BlockingCommit() = 0;

	virtual void				Watch(const std::string& key) = 0;
	virtual void				Multi() = 0;
	virtual void				Exec() = 0;

	virtual void				Dump(const std::string& key) = 0;
	virtual void				Restore(const std::string& key, std::string& val) = 0;

	virtual void				Set(const std::string& key, std::string& val) = 0;
	virtual void				Get(const std::string& key) = 0;
	virtual void				GetSet(const std::string& key, std::string& val) = 0;
	virtual void				Del(const std::vector<std::string>& vKey) = 0;

	virtual void				LPush(const std::string& key, std::vector<std::string>& vVal) = 0;
	virtual void				RPush(const std::string& key, std::vector<std::string>& vVal) = 0;
	virtual void				LPop(const std::string& key) = 0;
	virtual void				RPop(const std::string& key) = 0;
	virtual void				LLen(const std::string& key) = 0;
	virtual void				LIndex(const std::string& key, int nIndex) = 0;
	virtual void				LRange(const std::string& key, int nStart, int nStop) = 0;
	virtual void				LTrim(const std::string& key, int nStart, int nStop) = 0;

	virtual void				HKeys(const std::string& key) = 0;
	virtual void				HVals(const std::string& key) = 0;

	virtual void				HDel(const std::string& key, const std::vector<std::string>& vField) = 0;
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
	virtual void				Eval(const std::string& script, const std::vector<std::string>& vKey, std::vector<std::string>& vArg) = 0;
	virtual void				EvalSha(const std::string& sha, const std::vector<std::string>& vKey, std::vector<std::string>& vArg) = 0;

	virtual void				Shutdown() = 0;

};

class MY_REDIS_EXTERN IRedisSubscriber {
public:
	virtual ~IRedisSubscriber() noexcept {};

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

	virtual void				Shutdown() = 0;

};

/*EOF*/