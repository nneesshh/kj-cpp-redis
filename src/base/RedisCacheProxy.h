#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>

#include "platform_types.h"
#include "redis_service_def.h"
#include "RedisReply.h"

//------------------------------------------------------------------------------
/**
@class CRedisCacheProxy

(C) 2016 n.lee
*/
// walk online accounts
#define WALK_REDIS_REPLY_AS_RESULT_PAIR_BEGIN(_pair_list, _rkey, _rval)	\
	int __ncount = _pair_list.size() / 2;								\
	int __i;															\
	for (__i = 0; __i < __ncount; ++__i) {								\
		CRedisReply& __r1 = _pair_list[__i * 2];						\
		CRedisReply& __r2 = _pair_list[__i * 2 + 1];					\
		if (__r1.ok() && __r2.ok()										\
			&& __r1.is_string() && __r2.is_string()) {					\
			_rkey = &__r1.as_string();									\
			_rval = &__r2.as_string();									\
			if (true)

#define WALK_REDIS_REPLY_AS_RESULT_PAIR_END()							\
		}																\
	}

//////////////////////////////////////////////////////////////////////////
class CRedisHashTableBatchGetter;

//------------------------------------------------------------------------------
/**
@brief CRedisCacheProxy
*/
class MY_REDIS_EXTERN CRedisCacheProxy {
public:
	CRedisCacheProxy(void *service_entry, const char *sMainId, const char *sSubid = "1");
	CRedisCacheProxy(const std::string& sModuleName, const char *sMainId, const char *sSubid = "1");
	virtual ~CRedisCacheProxy();

	using RESULT_PAIR_LIST = std::vector<CRedisReply>;
	using DIRTY_ENTRY_LIST = std::vector<CRedisReply>;

	const std::string&			MainId() const {
		return _sMainId;
	}

	const std::string&			Subid() const {
		return _sSubid;
	}

	const std::string&			IdHash() const {
		return _sIdHash;
	}

	void						BindServiceEntry(void *service_entry) {
		_refEntry = service_entry;
	}

	void *						ServiceEntry() const {
		return _refEntry;
	}

	void						Commit();
	void						AddToHashTable(const std::string& sId, std::string& sValue);
	void						RemoveFromHashTable(const std::string& sId);
	void						UpdateToHashTable(const std::string& sId, std::string& sValue);

	void						Clear();

	void						Add(const std::string& sId, std::string& sValue) {
		AddToHashTable(sId, std::move(sValue));
		Commit();
	}

	void						Remove(const std::string& sId) {
		RemoveFromHashTable(sId);
		Commit();
	}

	void						Update(const std::string& sId, std::string& sValue) {
		UpdateToHashTable(sId, std::move(sValue));
		Commit();
	}
	
	const std::string			Get(const std::string& sId);
	void						GetAll(RESULT_PAIR_LIST& vOut);
	void						GetPartitial(int nCount, RESULT_PAIR_LIST& vOut);

public:
	static void					SplitIdHash(const std::string& sIdHash, std::string& sOutModuleName, std::string& sOutMainId, std::string& sOutSubid);
	static void					LootDirtyEntry(void *service_entry, DIRTY_ENTRY_LIST& vOut);

	static void					BatchGet(void *service_entry, CRedisHashTableBatchGetter& getter);
	static void					BatchGetAsync(void *service_entry, CRedisHashTableBatchGetter& getter);

	static const std::map<std::string, std::string>& MapScript();

private:
	void *_refEntry;

	std::string _sModuleName;
	std::string _sMainId;
	std::string _sSubid;
	std::string _sIdHash;
	std::string _sIdSetOfAllKeys;
	std::string _sIdSetOfDirtyKeys;

	friend CRedisHashTableBatchGetter;
};

//------------------------------------------------------------------------------
/**
@brief CRedisHashTableBatchGetter
*/
class CRedisHashTableBatchGetter {
public:
	CRedisHashTableBatchGetter() {
		_vKey.reserve(256);
		_vField.reserve(256);
		_vCb.reserve(256);
	}
	~CRedisHashTableBatchGetter() = default;

	using getter_cb_t = std::function<void(CRedisReply&)>;

	void Push(const CRedisCacheProxy& proxy, const std::string& sField, getter_cb_t cb) {
		_vKey.emplace_back(proxy._sIdHash);
		_vField.emplace_back(std::move(sField));
		_vCb.emplace_back(std::move(cb));
	}

	void Push(const CRedisCacheProxy& proxy, getter_cb_t cb) {
		Push(proxy, "*", std::move(cb));
	}

	std::vector<std::string> _vKey;
	std::vector<std::string> _vField;
	std::vector<getter_cb_t> _vCb;
};

/*EOF*/