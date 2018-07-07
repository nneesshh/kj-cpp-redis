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
	virtual void				RunOnce() override {
		_trunkQueue->RunOnce();
	}
	
	virtual void				Send(const std::vector<std::string>& vPiece) override;
	virtual void				Commit(redis_reply_cb_t&& reply_cb) override;
	virtual CRedisReply			BlockingCommit() override;

	virtual void				Watch(const std::string& key) override {
		Send({ "WATCH", key });
	}

	virtual void				Multi() override {
		Send({ "MULTI" });
	}

	virtual void				Exec() override {
		Send({ "EXEC" });
	}

	virtual void				Set(const std::string& key, std::string& val) override {
		Send({ "SET", key, std::move(val) });
	}

	virtual void				Get(const std::string& key) override {
		Send({ "GET", key });
	}

	virtual void				GetSet(const std::string& key, std::string& val) override {
		Send({ "GETSET", key, std::move(val) });
	}

	virtual void				Del(std::vector<std::string>& vKey) override {
		std::vector<std::string> vPiece(1 + vKey.size());
		vPiece.assign({ std::string("DEL") });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		Send(vPiece);
	}

	virtual void				LPush(const std::string& key, std::vector<std::string>& vVal) override {
		std::vector<std::string> vPiece(2 + vVal.size());
		vPiece.assign({ std::string("LPUSH"), key });
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		Send(vPiece);
	}

	virtual void				RPush(const std::string& key, std::vector<std::string>& vVal) override {
		std::vector<std::string> vPiece(2 + vVal.size());
		vPiece.assign({ "RPUSH", key });
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		Send(vPiece);
	}

	virtual void				LPop(const std::string& key) override {
		Send({ "LPOP", key });
	}

	virtual void				RPop(const std::string& key) override {
		Send({ "RPOP", key });
	}

	virtual void				LLen(const std::string& key) override {
		Send({ "LLEN", key });
	}

	virtual void				LIndex(const std::string& key, int nIndex) override {
		Send({ "LINDEX", key, std::to_string(nIndex) });
	}

	virtual void				LRange(const std::string& key, int nStart, int nStop) override {
		Send({ "LRANGE", key, std::to_string(nStart), std::to_string(nStop) });
	}

	virtual void				LTrim(const std::string& key, int nStart, int nStop) override {
		Send({ "LTRIM", key, std::to_string(nStart), std::to_string(nStop) });
	}

	virtual void				HDel(const std::string& key, std::vector<std::string>& vField) override {
		std::vector<std::string> vPiece(2 + vField.size());
		vPiece.assign({ "HDEL", key });
		vPiece.insert(vPiece.end(), vField.begin(), vField.end());
		Send(vPiece);
	}

	virtual void				HGetAll(const std::string& key) override {
		Send({ "HGETALL", key });
	}

	virtual void				HGet(const std::string& key, const std::string& field) override {
		Send({ "HGET", key, field });
	}

	virtual void				HSet(const std::string& key, const std::string& field, std::string& val) override {
		Send({ "HSET", key, field, std::move(val) });
	}

	virtual void				ZAdd(const std::string& key, std::string& score, std::string& member) override {
		Send({ "ZADD", key, score, member });
	}

	virtual void				ZRem(const std::string& key, std::vector<std::string>& vMember) override {
		std::vector<std::string> vPiece(2 + vMember.size());
		vPiece.assign({ "ZREM", key });
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		Send(vPiece);
	}

	virtual void				ZScore(const std::string& key, std::string& member) override {
		Send({ "ZSCORE", key, member });
	}

	virtual void				ZRange(const std::string& key, std::string& start, std::string& stop, bool bWithScores = false) override {
		if (bWithScores) {
			Send({ "ZRANGE", key, start, stop, "WITHSCORES" });
		}
		else {
			Send({ "ZRANGE", key, start, stop });
		}
	}

	virtual void				ZRank(const std::string& key, std::string& member) override {
		Send({ "ZRANK", key, member });
	}

	virtual void				SAdd(const std::string& key, std::vector<std::string>& vMember) override {
		std::vector<std::string> vPiece(2 + vMember.size());
		vPiece.assign({ "SADD", key });
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		Send(vPiece);
	}

	virtual void				SMembers(const std::string& key) override {
		Send({ "SMEMBERS", key });
	}

	virtual void				ScriptLoad(const std::string& script) override {
		Send({ "SCRIPT", "LOAD", script });
	}

	virtual void				Eval(const std::string& script, std::vector<std::string>& vKey, std::vector<std::string>& vArg) override {
		size_t szNumKeys = vKey.size();
		size_t szNumArgs = vArg.size();
		std::vector<std::string> vPiece(3 + szNumKeys + szNumArgs);
		vPiece.assign({ "EVAL", script, std::to_string(szNumKeys) });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
		Send(vPiece);
	}

	virtual void				EvalSha(const std::string& sha, std::vector<std::string>& vKey, std::vector<std::string>& vArg) override {
		size_t szNumKeys = vKey.size();
		size_t szNumArgs = vArg.size();
		std::vector<std::string> vPiece(3 + szNumKeys + szNumArgs);
		vPiece.assign({ "EVALSHA", sha, std::to_string(szNumKeys) });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
		Send(vPiece);
	}

	virtual void				Shutdown() override;

private:
	CRedisTrunkQueuePtr _trunkQueue;
	CKjRedisClientWorkQueuePtr _workQueue;

	std::string _singleCommand;
	std::string _allCommands;
	int _builtNum = 0;

	int _nextSn = 0;
};

/*EOF*/