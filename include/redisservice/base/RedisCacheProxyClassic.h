#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "../UsingDependency.h"
#include "IRedisHandler.h"

//------------------------------------------------------------------------------
/**
@class CRedisCacheProxy

(C) 2016 n.lee
*/

//------------------------------------------------------------------------------
/**
@brief CRedisCacheProxy
*/
class CRedisCacheProxy {
public:
	CRedisCacheProxy(IRedisHandler *pRedisHandler, const char *sCacheId, const char *sCacheSubid = "1", const char *sSuffixHash = "idhash", const char *sSuffixSetOfDirty = "idsetofdirty");
	virtual ~CRedisCacheProxy();

	using RESULT_MAP = std::unordered_map<std::string, std::string>;
	using on_got_result = std::function<void(const std::string&)>;
	using on_got_all = std::function<void(RESULT_MAP&)>;

	const std::string&			CacheId() {
		return _sCacheId;
	}

	const std::string&			CacheSubid() {
		return _sCacheSubid;
	}

	IRedisService&				RedisService() {
		return _refRedisHandler->RedisService();
	}

	void						Commit() {
		RedisService().Commit(_default_rcb);
	}

	void						AddToHashTable(const char *sId, std::string& sValue) {
		RedisService().HSet(_sIdHash, sId, sValue);
	}

	void						RemoveFromHashTable(const char *sId) {
		RedisService().HDel(_sIdHash, std::vector<std::string>{ sId });
	}

	void						UpdateToHashTable(const char *sId, std::string& sValue);
	void						Clear();

	void						Add(const char *sId, std::string& sValue) {
		AddToHashTable(sId, sValue);
		Commit();
	}

	void						Remove(const char *sId) {
		RemoveFromHashTable(sId);
		Commit();
	}

	void						Update(const char *sId, std::string& sValue) {
		UpdateToHashTable(sId, sValue);
		Commit();
	}
	
	const std::string			Get(const char *sId);
	void						Get(const char *sId, on_got_result cb);

	void						GetAll(RESULT_MAP& mapOut);
	void						GetAll(on_got_all cb);

	void						LootDirtyIds(std::vector<std::string>& vOut);

private:
	IRedisHandler *_refRedisHandler;
	IRedisService::reply_cb_t _default_rcb;

	std::string _sCacheId;
	std::string _sCacheSubid;
	std::string _sCacheName;
	std::string _sIdHash;
	std::string _sIdSetOfDirty;
};

/*EOF*/