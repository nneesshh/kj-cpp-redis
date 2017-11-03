#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisService

(C) 2016 n.lee
*/
#include "toolkit/framework/IRedisService.h"

#include "RedisTrunkQueue.h"
#include "kj/KjRedisWorkQueue.h"

//------------------------------------------------------------------------------
/**
@brief CRedisService
*/
class MY_EXTERN CRedisService : public IRedisService {
public:
	CRedisService(IAppBase *pApp, redis_stub_param_t *param);
	virtual ~CRedisService();

public:
	virtual void				RunOnce() {
		_trunkQueue->RunOnce();
	}

	virtual void				Send(const std::vector<std::string>& vPiece);
	virtual void				Commit(const reply_cb_t& reply_cb);
	virtual CRedisReply			BlockingCommit();

	virtual CRedisReply			LootHashTable(const std::string& strKey);
	virtual CRedisReply			LootHashTableField(const std::string& strKey, const std::string& strField);

	virtual void				Watch(const std::string& key) {
		Send({ "WATCH", key });
	}

	virtual void				Multi() {
		Send({ "MULTI" });
	}

	virtual void				Exec() {
		Send({ "EXEC" });
	}

	virtual void				Set(const std::string& key, const std::string& val) {
		Send({ "SET", key, val });
	}

	virtual void				Get(const std::string& key) {
		Send({ "GET", key });
	}

	virtual void				GetSet(const std::string& key, const std::string& val) {
		Send({ "GETSET", key, val });
	}

	virtual void				Del(const std::vector<std::string>& vKey) {
		std::vector<std::string> vPiece = { "DEL" };
		vPiece.insert(vPiece.end(), vKey.begin(), vKey.end());
		Send(vPiece);
	}

	virtual void				LPush(const std::string& key, const std::vector<std::string>& vVal) {
		std::vector<std::string> vPiece = { "LPUSH", key };
		vPiece.insert(vPiece.end(), vVal.begin(), vVal.end());
		Send(vPiece);
	}

	virtual void				RPush(const std::string& key, const std::vector<std::string>& vVal) {
		std::vector<std::string> vPiece = { "RPUSH", key };
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

	virtual void				HDel(const std::string& key, const std::vector<std::string>& vField) {
		std::vector<std::string> vPiece = { "HDEL", key };
		vPiece.insert(vPiece.end(), vField.begin(), vField.end());
		Send(vPiece);
	}

	virtual void				HGetAll(const std::string& key) {
		Send({ "HGETALL", key });
	}

	virtual void				HGet(const std::string& key, const std::string& field) {
		Send({ "HGET", key, field });
	}

	virtual void				HSet(const std::string& key, const std::string& field, const std::string& val) {
		Send({ "HSET", key, field, val });
	}

	virtual void				ZAdd(const std::string& key, const std::string& score, const std::string& member) {
		Send({ "ZADD", key, score, member });
	}

	virtual void				ZRem(const std::string& key, const std::vector<std::string>& vMember) {
		std::vector<std::string> vPiece = { "ZREM", key };
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		Send(vPiece);
	}

	virtual void				ZRange(const std::string& key, const std::string& start, const std::string& stop, bool bWithScores = false) {
		if (bWithScores) {
			Send({ "ZRANGE", key, start, stop, "WITHSCORES" });
		}
		else {
			Send({ "ZRANGE", key, start, stop });
		}
	}

	virtual void				ZRank(const std::string& key, const std::string& member) {
		Send({ "ZRANK", key, member });
	}

	virtual void				SAdd(const std::string& key, const std::vector<std::string>& vMember) {
		std::vector<std::string> vPiece = { "SADD", key };
		vPiece.insert(vPiece.end(), vMember.begin(), vMember.end());
		Send(vPiece);
	}

	virtual void				SMembers(const std::string& key) {
		Send({ "SMEMBERS", key });
	}

private:
	std::string&				BuildCommand(const std::vector<std::string>& vPiece) {
		_singleCommand.resize(0);
		_singleCommand.append("*").append(std::to_string(vPiece.size())).append("\r\n");

		for (const auto& piece : vPiece) {
			_singleCommand.append("$").append(std::to_string(piece.length())).append("\r\n").append(piece).append("\r\n");
		}
		return _singleCommand;
	}

private:
	IAppBase *_refApp;

	redis_stub_param_t _param;

	int _nextSn = 0;
	std::string _singleCommand;
	std::string _allCommands;
	int _builtNum = 0;

	CRedisTrunkQueuePtr _trunkQueue;
	CKjRedisWorkQueuePtr _workQueue;
};

/*EOF*/