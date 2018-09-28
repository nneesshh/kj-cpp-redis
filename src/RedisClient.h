#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisClient

(C) 2016 n.lee
*/
#include "base/IRedisService.h"

#include "io/RedisTrunkQueue.h"
#include "io/KjRedisClientWorkQueue.hpp"

#include "RedisCommandBuilder.h"

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
	
	virtual void				Commit(redis_reply_cb_t&& rcb) override;
	virtual CRedisReply			BlockingCommit() override;

	virtual void				Watch(const std::string& key) override {
		BuildCommand({ "WATCH", key });
	}

	virtual void				Multi() override {
		BuildCommand({ "MULTI" });
	}

	virtual void				Exec() override {
		BuildCommand({ "EXEC" });
	}

	virtual void				Dump(const std::string& key) override {
		BuildCommand({ "DUMP", key });
	}

	virtual void				Restore(const std::string& key, std::string& val) override {
		BuildCommand({ "RESTORE", key, std::move(val) });
	}

	virtual void				Set(const std::string& key, std::string& val) override {
		BuildCommand({ "SET", key, std::move(val) });
	}

	virtual void				Get(const std::string& key) override {
		BuildCommand({ "GET", key });
	}

	virtual void				GetSet(const std::string& key, std::string& val) override {
		BuildCommand({ "GETSET", key, std::move(val) });
	}

	virtual void				Del(const std::vector<std::string>& vKey) override {
		std::vector<std::string> vPiece(1 + vKey.size());
		vPiece.assign({ std::string("DEL") });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		BuildCommand(vPiece);
	}

	virtual void				LPush(const std::string& key, std::vector<std::string>& vVal) override {
		std::vector<std::string> vPiece(2 + vVal.size());
		vPiece.assign({ std::string("LPUSH"), key });
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		BuildCommand(vPiece);
	}

	virtual void				RPush(const std::string& key, std::vector<std::string>& vVal) override {
		std::vector<std::string> vPiece(2 + vVal.size());
		vPiece.assign({ "RPUSH", key });
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		BuildCommand(vPiece);
	}

	virtual void				LPop(const std::string& key) override {
		BuildCommand({ "LPOP", key });
	}

	virtual void				RPop(const std::string& key) override {
		BuildCommand({ "RPOP", key });
	}

	virtual void				LLen(const std::string& key) override {
		BuildCommand({ "LLEN", key });
	}

	virtual void				LIndex(const std::string& key, int nIndex) override {
		BuildCommand({ "LINDEX", key, std::to_string(nIndex) });
	}

	virtual void				LRange(const std::string& key, int nStart, int nStop) override {
		BuildCommand({ "LRANGE", key, std::to_string(nStart), std::to_string(nStop) });
	}

	virtual void				LTrim(const std::string& key, int nStart, int nStop) override {
		BuildCommand({ "LTRIM", key, std::to_string(nStart), std::to_string(nStop) });
	}

	virtual void				HKeys(const std::string& key) override {
		BuildCommand({ "HKEYS", key });
	}

	virtual void				HVals(const std::string& key) override {
		BuildCommand({ "HVals", key });
	}

	virtual void				HDel(const std::string& key, const std::vector<std::string>& vField) override {
		std::vector<std::string> vPiece(2 + vField.size());
		vPiece.assign({ "HDEL", key });
		vPiece.insert(vPiece.end(), vField.begin(), vField.end());
		BuildCommand(vPiece);
	}

	virtual void				HGetAll(const std::string& key) override {
		BuildCommand({ "HGETALL", key });
	}

	virtual void				HGet(const std::string& key, const std::string& field) override {
		BuildCommand({ "HGET", key, field });
	}

	virtual void				HSet(const std::string& key, const std::string& field, std::string& val) override {
		BuildCommand({ "HSET", key, field, std::move(val) });
	}

	virtual void				ZAdd(const std::string& key, std::string& score, std::string& member) override {
		BuildCommand({ "ZADD", key, score, member });
	}

	virtual void				ZRem(const std::string& key, std::vector<std::string>& vMember) override {
		std::vector<std::string> vPiece(2 + vMember.size());
		vPiece.assign({ "ZREM", key });
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		BuildCommand(vPiece);
	}

	virtual void				ZScore(const std::string& key, std::string& member) override {
		BuildCommand({ "ZSCORE", key, member });
	}

	virtual void				ZRange(const std::string& key, std::string& start, std::string& stop, bool bWithScores = false) override {
		if (bWithScores) {
			BuildCommand({ "ZRANGE", key, start, stop, "WITHSCORES" });
		}
		else {
			BuildCommand({ "ZRANGE", key, start, stop });
		}
	}

	virtual void				ZRank(const std::string& key, std::string& member) override {
		BuildCommand({ "ZRANK", key, member });
	}

	virtual void				SAdd(const std::string& key, std::vector<std::string>& vMember) override {
		std::vector<std::string> vPiece(2 + vMember.size());
		vPiece.assign({ "SADD", key });
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		BuildCommand(vPiece);
	}

	virtual void				SMembers(const std::string& key) override {
		BuildCommand({ "SMEMBERS", key });
	}

	virtual void				ScriptLoad(const std::string& script) override {
		BuildCommand({ "SCRIPT", "LOAD", script });
	}

	virtual void				Eval(const std::string& script, const std::vector<std::string>& vKey, std::vector<std::string>& vArg) override {
		size_t szNumKeys = vKey.size();
		size_t szNumArgs = vArg.size();
		std::vector<std::string> vPiece(3 + szNumKeys + szNumArgs);
		vPiece.assign({ "EVAL", script, std::to_string(szNumKeys) });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
		BuildCommand(vPiece);
	}

	virtual void				EvalSha(const std::string& sha, const std::vector<std::string>& vKey, std::vector<std::string>& vArg) override {
		size_t szNumKeys = vKey.size();
		size_t szNumArgs = vArg.size();
		std::vector<std::string> vPiece(3 + szNumKeys + szNumArgs);
		vPiece.assign({ "EVALSHA", sha, std::to_string(szNumKeys) });
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		vPiece.insert(vPiece.end(), vArg.begin(), vArg.end());
		BuildCommand(vPiece);
	}

	virtual void				Shutdown() override;

private:
	void						BuildCommand(const std::vector<std::string>& vPiece) {
		CRedisCommandBuilder::Build(vPiece, _singleCommand, _allCommands, _builtNum);
		_singleCommand.resize(0);
	}

private:
	redis_stub_param_t& _refParam;

	CRedisTrunkQueuePtr _trunkQueue;
	CKjRedisClientWorkQueuePtr _workQueue;

	std::string _singleCommand;
	std::string _allCommands;
	int _builtNum = 0;

	int _nextSn = 0;
};

/*EOF*/