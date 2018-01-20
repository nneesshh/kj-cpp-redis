//------------------------------------------------------------------------------
//  RedisRankingProxy.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisRankingProxy.h"

#include "redis_service_def.h"

static std::string s_sClear = "deb71530a8a2f6470a74c33cc9b3b3bba6fcc69e";

//////////////////////////////////////////////////////////////////////////
static std::map<std::string, std::string> s_mapScript = {
	{ s_sClear,	"local r=redis.call('HGET',KEYS[1],'is_dirty');if (type(r)=='boolean' and not r or nil==r or ''==r)or(0==tonumber(r)) then redis.call('HDEL',KEYS[3],KEYS[2]);redis.call('DEL',KEYS[2]);redis.call('DEL',KEYS[1]);return true;else return false;end" },
};

//------------------------------------------------------------------------------
/**

*/
CRedisRankingProxy::CRedisRankingProxy(void *entry, const char *sMainId, const char *sSubid)
	: _refEntry(entry)
	, _sModuleName((static_cast<redis_service_entry_t *>(entry))->_sModuleName)
	, _sMainId(sMainId)
	, _sSubid(sSubid)
	, _sIdZSet(_sModuleName + ":" + _sMainId + ":" + sSubid + ":Z")
	, _sIdHashOfCAS(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":CAS_H") {

}

//------------------------------------------------------------------------------
/**

*/
CRedisRankingProxy::CRedisRankingProxy(const std::string& sModuleName, const char *sMainId, const char *sSubid)
	: _refEntry(nullptr)
	, _sModuleName(sModuleName)
	, _sMainId(sMainId)
	, _sSubid(sSubid)
	, _sIdZSet(_sModuleName + ":" + _sMainId + ":" + sSubid + ":Z")
	, _sIdHashOfCAS(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":CAS_H") {
	
}

//------------------------------------------------------------------------------
/**

*/
CRedisRankingProxy::~CRedisRankingProxy() {

}

//------------------------------------------------------------------------------
/**

*/
void
CRedisRankingProxy::Commit() {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redis_reply_cb_t defaultCb = [](CRedisReply&) {};
	redisservice->Client().Commit(std::move(defaultCb));
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisRankingProxy::AddToZSet(double dScore, std::string& sMember) {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().ZAdd(_sIdZSet.c_str(), std::to_string(dScore), sMember);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisRankingProxy::RemoveFromZSet(std::vector<std::string>& vMember) {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().ZRem(_sIdZSet.c_str(), vMember);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisRankingProxy::ZScore(std::string& sMember) {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().ZScore(_sIdZSet.c_str(), sMember);
};

//------------------------------------------------------------------------------
/**

*/
void
CRedisRankingProxy::Clear() {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sClear,
		std::vector<std::string>{ _sIdZSet },
		std::vector<std::string>{ }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_null()) {
		// null means false
		fprintf(stderr, "[CRedisRankingProxy::Clear()] Ranking(%s) is dirty, you must clean it first before -- Clear() --!!!!",
			_sIdZSet.c_str());
		system("pause");
		exit(-1);
	}
}

//------------------------------------------------------------------------------
/**

*/
double
CRedisRankingProxy::GetScore(std::string& sMember) {
	
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().ZScore(_sIdZSet.c_str(), sMember);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_string()) {
		//
		return atof(reply.as_string().c_str());
	}
	return DBL_MIN;
}

//------------------------------------------------------------------------------
/**

*/
const std::map<std::string, std::string>&
CRedisRankingProxy::MapScript() {
	return s_mapScript;
}

/* -- EOF -- */