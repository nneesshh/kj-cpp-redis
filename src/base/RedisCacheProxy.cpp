//------------------------------------------------------------------------------
//  RedisCacheProxy.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisCacheProxy.h"

#include "redis_service_def.h"
#include "IRedisService.h"

static int
__split(const char *str, int str_len, char **av, int av_max, char c) {
	int i, j;
	char *ptr = (char*)str;
	int count = 0;

	if (!str_len) str_len = strlen(ptr);

	for (i = 0, j = 0; i < str_len&& count < av_max; ++i) {
		if (ptr[i] != c)
			continue;
		else
			ptr[i] = 0x0;

		av[count++] = &(ptr[j]);
		j = i + 1;
		continue;
	}

	if (j < i) av[count++] = &(ptr[j]);
	return count;
}

static std::string s_sClear = "a6329c4a13520533c3cb11d14bad6603016b7ca7";
static std::string s_sAddToHashTable = "9f806238b7adb46f45e795c1f02371039a1a9d83";
static std::string s_sUpdateToHashTable = "b13e9be373badb963061cddb27d1a574d7a383cd";
static std::string s_sRemoveFromHashTable = "40da6061ff5b171ff59bc8ba5f3a91ca6b7c88be";

static std::string s_sGetPartitial = "d3c6f9a0c40a897d722d01b087903d0f45738ffb";

static std::string s_sLootDirtyEntry = "a0894ce0a0e6be069f86fb26a8a8f622c223fe31";

static std::string s_sBatchGet = "d8bceba1a105c00d38754a77425aa4933539e69c";

//////////////////////////////////////////////////////////////////////////
static std::map<std::string, std::string> s_mapScript = {
	{ s_sClear,					"local n=redis.call('HLEN',KEYS[3]);if n>0 then return false;else redis.call('HDEL',KEYS[4],KEYS[2]);redis.call('DEL',KEYS[3]);redis.call('DEL',KEYS[2]);redis.call('DEL',KEYS[1]);return true;end" },
	{ s_sAddToHashTable,		"redis.call('HSET',KEYS[1],ARGV[1],ARGV[2]);redis.call('HSET',KEYS[2],ARGV[1],ARGV[2]);redis.call('HSET',KEYS[3],ARGV[1],4);redis.call('HSET',KEYS[4],KEYS[2],KEYS[3])" }, // Added = 4
	{ s_sUpdateToHashTable,		"redis.call('HSET',KEYS[1],ARGV[1],ARGV[2]);redis.call('HSET',KEYS[2],ARGV[1],ARGV[2]);redis.call('HSET',KEYS[3],ARGV[1],3);redis.call('HSET',KEYS[4],KEYS[2],KEYS[3])" }, // Modified = 3
	{ s_sRemoveFromHashTable,	"local v;v=redis.call('HGET',KEYS[1],ARGV[1]);redis.call('HDEL',KEYS[1],ARGV[1]);redis.call('HSET',KEYS[2],ARGV[1],v);redis.call('HSET',KEYS[3],ARGV[1],2);redis.call('HSET',KEYS[4],KEYS[2],KEYS[3])" }, // Deleted = 2

	{ s_sGetPartitial,			"local r,p;r={};p=redis.call('HKEYS',KEYS[1]);for i=1,ARGV[1] do v=p[i];table.insert(r,v);table.insert(r,redis.call('HGET',KEYS[1],v));end;return r" },

	{ s_sLootDirtyEntry,		"local r,e,n,i,k,v,d,s,t;r={};e=redis.call('HGETALL',KEYS[1]);redis.call('DEL',KEYS[1]);n=(e and #e) or 0;for i=1,n-1,2 do k=e[i];v=e[i+1];d=redis.call('DUMP',k);redis.call('DEL',k);s=redis.call('DUMP',v);redis.call('DEL',v);t={v,d,s};table.insert(r,t);end;return r" },

	{ s_sBatchGet,				"local r,i,v;r={};for i,v in ipairs(KEYS) do if '*'==ARGV[i] then table.insert(r,redis.call('HGETALL',v)) else table.insert(r,redis.call('HGET',v,ARGV[i])) end;end;return r" },
};

//------------------------------------------------------------------------------
/**

*/
CRedisCacheProxy::CRedisCacheProxy(void *service_entry, const char *sMainId, const char *sSubid)
	: _refEntry(service_entry)
	, _sModuleName((static_cast<redis_service_entry_t *>(service_entry))->_sModuleName)
	, _sMainId(sMainId)
	, _sSubid(sSubid)
	, _sIdHash(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":H")
	, _sIdHashOfDirty(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":D_H")
	, _sIdHashOfDirtyState(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":DS_H") {
	
}

//------------------------------------------------------------------------------
/**

*/
CRedisCacheProxy::CRedisCacheProxy(const std::string& sModuleName, const char *sMainId, const char *sSubid)
	: _refEntry(nullptr)
	, _sModuleName(sModuleName)
	, _sMainId(sMainId)
	, _sSubid(sSubid)
	, _sIdHash(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":H")
	, _sIdHashOfDirty(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":D_H")
	, _sIdHashOfDirtyState(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":DS_H") {

}

//------------------------------------------------------------------------------
/**

*/
CRedisCacheProxy::~CRedisCacheProxy() {
	
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::Clear() {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sClear,
		std::vector<std::string>{ _sIdHash, _sIdHashOfDirty, _sIdHashOfDirtyState, entry->_cacheDirtyEntry },
		std::vector<std::string>{ }
	);
	
	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_null()) {
		// null means false
		fprintf(stderr, "[CRedisCacheProxy::Clear()] cache(%s) is dirty, you must dump the changes to db first before clear it!!!!",
			_sIdHash.c_str());
		system("pause");
		exit(-1);
	}
	else if (reply.is_error()) {
		std::string sDesc = "[CRedisCacheProxy::Clear()] error(";
		sDesc += reply.error_desc().c_str();
		sDesc += ")!!!";
		throw std::exception(sDesc.c_str());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::Commit() {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().Commit(nullptr);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::SetToHashTable(const std::string& sId, std::string& sValue) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().HSet(_sIdHash.c_str(), sId.c_str(), sValue);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::AddToHashTable(const std::string& sId, std::string& sValue) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sAddToHashTable,
		std::vector<std::string>{ _sIdHash, _sIdHashOfDirty, _sIdHashOfDirtyState, entry->_cacheDirtyEntry },
		std::vector<std::string>{ sId, std::move(sValue) }
	);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::RemoveFromHashTable(const std::string& sId) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sRemoveFromHashTable,
		std::vector<std::string>{ _sIdHash, _sIdHashOfDirty, _sIdHashOfDirtyState, entry->_cacheDirtyEntry },
		std::vector<std::string>{ sId }
	);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::UpdateToHashTable(const std::string& sId, std::string& sValue) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sUpdateToHashTable,
		std::vector<std::string>{ _sIdHash, _sIdHashOfDirty, _sIdHashOfDirtyState, entry->_cacheDirtyEntry },
		std::vector<std::string>{ sId, std::move(sValue) }
	);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::Set(const std::string& sId, std::string& sValue) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().HSet(_sIdHash.c_str(), sId.c_str(), sValue);
	redisservice->Client().Commit(nullptr);
}

//------------------------------------------------------------------------------
/**

*/
const std::string
CRedisCacheProxy::Get(const std::string& sId) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().HGet(_sIdHash.c_str(), sId.c_str());

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_string()) {
		//
		return reply.as_string();
	}
	else if (reply.is_error()) {
		std::string sDesc = "[CRedisCacheProxy::Get()] id(";
		sDesc += sId.c_str();
		sDesc += ") -- error(";
		sDesc += reply.error_desc().c_str();
		sDesc += ")!!!";
		throw std::exception(sDesc.c_str());
	}
	return "";
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::GetAll(RESULT_PAIR_LIST& vOut) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().HGetAll(_sIdHash.c_str());

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
	else if (reply.is_error()) {
		std::string sDesc = "[CRedisCacheProxy::GetAll()] error(";
		sDesc += reply.error_desc().c_str();
		sDesc += ")!!!";
		throw std::exception(sDesc.c_str());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::GetPartitial(int nCount, RESULT_PAIR_LIST& vOut) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sGetPartitial,
		std::vector<std::string>{ _sIdHash },
		std::vector<std::string>{ std::to_string(nCount) }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
	else if (reply.is_error()) {
		std::string sDesc = "[CRedisCacheProxy::GetPartitial()] error(";
		sDesc += reply.error_desc().c_str();
		sDesc += ")!!!";
		throw std::exception(sDesc.c_str());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::SplitIdHash(const std::string& sIdHash, std::string& sOutModuleName, std::string& sOutMainId, std::string& sOutSubid) {
	std::string sTmp = sIdHash;
	char* chArr[8] = { 0 };
	int n = __split(sTmp.c_str(), sTmp.length(), chArr, 8, ':');
	if (n >= 3) {
		sOutModuleName = chArr[0];
		sOutMainId = chArr[1];
		sOutSubid = chArr[2];
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::LootDirtyEntry(void *service_entry, DIRTY_ENTRY_LIST& vOut) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(service_entry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sLootDirtyEntry,
		std::vector<std::string>{ entry->_cacheDirtyEntry },
		std::vector<std::string>{ }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
	else if (reply.is_error()) {
		std::string sDesc = "[CRedisCacheProxy::LootDirtyEntry()] error(";
		sDesc += reply.error_desc().c_str();
		sDesc += ")!!!";
		throw std::exception(sDesc.c_str());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::BatchGet(void *service_entry, CRedisHashTableBatchGetter& getter) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(service_entry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sBatchGet,
		getter._vKey,
		getter._vField
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		std::vector<CRedisReply>& v = reply.as_array();
		int i;
		for (i = 0; i < v.size(); ++i) {
			// callback
			getter._vCb[i](v[i]);
		}
	}
	else if (reply.is_error()) {
		std::string sDesc = "[CRedisCacheProxy::BatchGet()] error(";
		sDesc += reply.error_desc().c_str();
		sDesc += ")!!!";
		throw std::exception(sDesc.c_str());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::BatchGetAsync(void *service_entry, CRedisHashTableBatchGetter& getter) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(service_entry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redis_reply_cb_t rcb = std::bind([&getter](std::vector<CRedisHashTableBatchGetter::getter_cb_t>& vCb, CRedisReply& reply) {
		if (reply.ok()
			&& reply.is_array()) {
			//
			std::vector<CRedisReply>& v = reply.as_array();
			int i;
			for (i = 0; i < v.size(); ++i) {
				// callback
				vCb[i](v[i]);
			}
		}
		else if (reply.is_error()) {
			std::string sDesc = "[CRedisCacheProxy::BatchGetAsync()] error(";
			sDesc += reply.error_desc().c_str();
			sDesc += ")!!!";
			throw std::exception(sDesc.c_str());
		}

	}, std::move(getter._vCb), std::move(std::placeholders::_1));

	redisservice->Client().EvalSha(
		s_sBatchGet,
		getter._vKey,
		getter._vField
	);
	redisservice->Client().Commit(std::move(rcb));

}

//------------------------------------------------------------------------------
/**

*/
const std::map<std::string, std::string>&
CRedisCacheProxy::MapScript() {
	return s_mapScript;
}

/* -- EOF -- */