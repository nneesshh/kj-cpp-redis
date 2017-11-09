#pragma once
//------------------------------------------------------------------------------
/**
@class IRedisService

(C) 2016 n.lee
*/
#include <vector>
#include <string>
#include <functional>

#include "RedisReply.h"

struct redis_stub_param_t {
	std::string _ip;
	unsigned short _port;

	std::string _preloadFlagEntry;
	std::string _dirtyEntry;
	int _dumpInterval = 300;

	std::string	_queryCacheKey;
	std::string	_updateCacheKey;
};

class IRedisService {
public:
	virtual ~IRedisService() {};

	using reply_cb_t = std::function<void(CRedisReply&&)>;
	using dispose_cb_t = std::function<void()>;

	struct cmd_pipepline_t {
		enum CMD_PIPELINE_STATE {
			CMD_PIPELINE_STATE_QUEUEING = 1,
			CMD_PIPELINE_STATE_SENDING = 2,
			CMD_PIPELINE_STATE_COMMITING = 3,
			CMD_PIPELINE_STATE_PROCESSING = 4,
			CMD_PIPELINE_STATE_PROCESS_OVER = 5,
		};

		int _sn;
		std::string _commands;
		int _built_num;
		int _processed_num;
		reply_cb_t _reply_cb;
		dispose_cb_t _dispose_cb;
		CMD_PIPELINE_STATE _state;
	};

	virtual void				RunOnce() = 0;

	virtual void				Send(const std::vector<std::string>& vInputPiece) = 0;
	virtual void				Commit(const reply_cb_t& reply_cb) = 0;
	virtual CRedisReply			BlockingCommit() = 0;

	virtual CRedisReply			LootHashTable(const std::string& strKey) = 0;
	virtual CRedisReply			LootHashTableField(const std::string& strKey, const std::string& strField) = 0;

	virtual void				Watch(const std::string& key) = 0;
	virtual void				Multi() = 0;
	virtual void				Exec() = 0;

	virtual void				Set(const std::string& key, const std::string& val) = 0;
	virtual void				Get(const std::string& key) = 0;
	virtual void				GetSet(const std::string& key, const std::string& val) = 0;
	virtual void				Del(const std::vector<std::string>& vKey) = 0;

	virtual void				LPush(const std::string& key, const std::vector<std::string>& vVal) = 0;
	virtual void				RPush(const std::string& key, const std::vector<std::string>& vVal) = 0;
	virtual void				LPop(const std::string& key) = 0;
	virtual void				RPop(const std::string& key) = 0;
	virtual void				LLen(const std::string& key) = 0;
	virtual void				LIndex(const std::string& key, int nIndex) = 0;
	virtual void				LRange(const std::string& key, int nStart, int nStop) = 0;
	virtual void				LTrim(const std::string& key, int nStart, int nStop) = 0;

	virtual void				HDel(const std::string& key, const std::vector<std::string>& vField) = 0;
	virtual void				HGetAll(const std::string& key) = 0;
	virtual void				HGet(const std::string& key, const std::string& field) = 0;
	virtual void				HSet(const std::string& key, const std::string& field, const std::string& val) = 0;

	virtual void				ZAdd(const std::string& key, const std::string& score, const std::string& member) = 0;
	virtual void				ZRem(const std::string& key, const std::vector<std::string>& vMember) = 0;
	virtual void				ZRange(const std::string& key, const std::string& start, const std::string& stop, bool bWithScores = false) = 0;
	virtual void				ZRank(const std::string& key, const std::string& member) = 0;

	virtual void				SAdd(const std::string& key, const std::vector<std::string>& vMember) = 0;
	virtual void				SMembers(const std::string& key) = 0;

	virtual void				Release() = 0;

};

/*EOF*/