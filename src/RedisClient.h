#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisClient

(C) 2016 n.lee
*/
#include "base/IRedisService.h"

#include "io/RedisTrunkQueue.h"
#include "io/KjRedisClientWorkQueue.hpp"

//------------------------------------------------------------------------------
/**
@brief CRedisClient
*/
class CRedisClient : public IRedisClient {
public:
	explicit CRedisClient(kj::Own<KjSimpleIoContext> rootContext, redis_stub_param_t& param);
	virtual ~CRedisClient();

public:
	virtual void				RunOnce() {
		_trunkQueue->RunOnce();
	}
	
	virtual void				Send(const std::vector<std::string>& vPiece);
	virtual void				Commit(redis_reply_cb_t&& reply_cb);
	virtual CRedisReply			BlockingCommit();

	virtual void				Watch(const std::string& key) {
		Send({ "WATCH", key });
	}

	virtual void				Multi() {
		Send({ "MULTI" });
	}

	virtual void				Exec() {
		Send({ "EXEC" });
	}

	virtual void				Set(const std::string& key, std::string& val) {
		Send({ "SET", key, std::move(val) });
	}

	virtual void				Get(const std::string& key) {
		Send({ "GET", key });
	}

	virtual void				GetSet(const std::string& key, std::string& val) {
		Send({ "GETSET", key, std::move(val) });
	}

	virtual void				Del(std::vector<std::string>& vKey) {
		std::vector<std::string> vPiece(1 + vKey.size());
		vPiece.assign({ std::string("DEL") });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		Send(vPiece);
	}

	virtual void				LPush(const std::string& key, std::vector<std::string>& vVal) {
		std::vector<std::string> vPiece(2 + vVal.size());
		vPiece.assign({ std::string("LPUSH"), key });
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		Send(vPiece);
	}

	virtual void				RPush(const std::string& key, std::vector<std::string>& vVal) {
		std::vector<std::string> vPiece(2 + vVal.size());
		vPiece.assign({ "RPUSH", key });
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		Send(vPiece);
	}

	virtual void				LPop(const std::string& key) {
		Send({ "LPOP", key });
	}

	virtual void				RPop(const std::string& key) {
		Send({ "RPOP", key });
	}

	virtual void				LLen(const std::string& key) {
		Send({ "LLEN", key });
	}

	virtual void				LIndex(const std::string& key, int nIndex) {
		Send({ "LINDEX", key, std::to_string(nIndex) });
	}

	virtual void				LRange(const std::string& key, int nStart, int nStop) {
		Send({ "LRANGE", key, std::to_string(nStart), std::to_string(nStop) });
	}

	virtual void				LTrim(const std::string& key, int nStart, int nStop) {
		Send({ "LTRIM", key, std::to_string(nStart), std::to_string(nStop) });
	}

	virtual void				HDel(const std::string& key, std::vector<std::string>& vField) {
		std::vector<std::string> vPiece(2 + vField.size());
		vPiece.assign({ "HDEL", key });
		vPiece.insert(vPiece.end(), vField.begin(), vField.end());
		Send(vPiece);
	}

	virtual void				HGetAll(const std::string& key) {
		Send({ "HGETALL", key });
	}

	virtual void				HGet(const std::string& key, const std::string& field) {
		Send({ "HGET", key, field });
	}

	virtual void				HSet(const std::string& key, const std::string& field, std::string& val) {
		Send({ "HSET", key, field, std::move(val) });
	}

	virtual void				ZAdd(const std::string& key, std::string& score, std::string& member) {
		Send({ "ZADD", key, score, member });
	}

	virtual void				ZRem(const std::string& key, std::vector<std::string>& vMember) {
		std::vector<std::string> vPiece(2 + vMember.size());
		vPiece.assign({ "ZREM", key });
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		Send(vPiece);
	}

	virtual void				ZScore(const std::string& key, std::string& member) {
		Send({ "ZSCORE", key, member });
	}

	virtual void				ZRange(const std::string& key, std::string& start, std::string& stop, bool bWithScores = false) {
		if (bWithScores) {
			Send({ "ZRANGE", key, start, stop, "WITHSCORES" });
		}
		else {
			Send({ "ZRANGE", key, start, stop });
		}
	}

	virtual void				ZRank(const std::string& key, std::string& member) {
		Send({ "ZRANK", key, member });
	}

	virtual void				SAdd(const std::string& key, std::vector<std::string>& vMember) {
		std::vector<std::string> vPiece(2 + vMember.size());
		vPiece.assign({ "SADD", key });
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		Send(vPiece);
	}

	virtual void				SMembers(const std::string& key) {
		Send({ "SMEMBERS", key });
	}

	virtual void				ScriptLoad(const std::string& script) {
		Send({ "SCRIPT", "LOAD", script });
	}

	virtual void				Eval(const std::string& script, std::vector<std::string>& vKey, std::vector<std::string>& vArg) {
		size_t szNumKeys = vKey.size();
		size_t szNumArgs = vArg.size();
		std::vector<std::string> vPiece(3 + szNumKeys + szNumArgs);
		vPiece.assign({ "EVAL", script, std::to_string(szNumKeys) });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
		Send(vPiece);
	}

	virtual void				EvalSha(const std::string& sha, std::vector<std::string>& vKey, std::vector<std::string>& vArg) {
		size_t szNumKeys = vKey.size();
		size_t szNumArgs = vArg.size();
		std::vector<std::string> vPiece(3 + szNumKeys + szNumArgs);
		vPiece.assign({ "EVALSHA", sha, std::to_string(szNumKeys) });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
		Send(vPiece);
	}

private:
	CRedisTrunkQueuePtr _trunkQueue;
	CKjRedisClientWorkQueuePtr _workQueue;

	std::string _singleCommand;
	std::string _allCommands;
	int _builtNum = 0;

	int _nextSn = 0;
};

/*EOF*/