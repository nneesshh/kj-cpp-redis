//------------------------------------------------------------------------------
//  RedisCacheProxy.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisCacheProxy.h"

//------------------------------------------------------------------------------
/**

*/
CRedisCacheProxy::CRedisCacheProxy(IRedisHandler *pRedisHandler, const char *sCacheId, const char *sCacheSubid, const char *sSuffixHash, const char *sSuffixSetOfDirty)
	: _refRedisHandler(pRedisHandler)
	, _sCacheId(sCacheId)
	, _sCacheSubid(sCacheSubid)
	, _sCacheName(_sCacheId + ":" + sCacheSubid)
	, _sIdHash(pRedisHandler->GetModuleName() + ":" + _sCacheName + ":" + sSuffixHash)
	, _sIdSetOfDirty(pRedisHandler->GetModuleName() + ":" + _sCacheName + ":" + sSuffixSetOfDirty) {
	//
	_default_rcb = [](CRedisReply&) {};
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
	IRedisService& redisservice = _refRedisHandler->RedisService();
	redis_stub_param_t& rs_param = _refRedisHandler->RedisStubParam();
	std::string& sDirtyEntry = rs_param._dirtyEntry;

	redisservice.Watch(_sIdSetOfDirty);
	redisservice.Multi();
	{
		redisservice.Del(std::vector<std::string>{ _sIdHash });
		redisservice.Del(std::vector<std::string>{ _sIdSetOfDirty });
		redisservice.HDel(sDirtyEntry, std::vector<std::string>{ _sCacheName });
	}
	redisservice.Exec();

	Commit();
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::UpdateToHashTable(const char *sId, std::string& sValue) {
	IRedisService& redisservice = _refRedisHandler->RedisService();
	redis_stub_param_t& rs_param = _refRedisHandler->RedisStubParam();
	std::string& sDirtyEntry = rs_param._dirtyEntry;
	
	redisservice.Multi();
	{
		// add to hash table
		redisservice.HSet(_sIdHash, sId, sValue);

		// mark dirty
		redisservice.SAdd(_sIdSetOfDirty, std::vector<std::string>{ sId });
		redisservice.HSet(sDirtyEntry, _sCacheName, _sCacheId);
	}
	redisservice.Exec();
}

//------------------------------------------------------------------------------
/**

*/
const std::string
CRedisCacheProxy::Get(const char *sId) {
	IRedisService& redisservice = _refRedisHandler->RedisService();

	redisservice.HGet(_sIdHash, sId);

	CRedisReply reply = redisservice.BlockingCommit();
	if (reply.ok()
		&& !reply.is_null()
		&& reply.is_string()) {
		//
		return std::move(reply.as_string());
	}
	return "";
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::Get(const char *sId, on_got_result cb) {
	IRedisService& redisservice = _refRedisHandler->RedisService();
	IRedisService::reply_cb_t rcb = [&cb](CRedisReply& reply) {
		if (reply.ok()
			&& !reply.is_null()
			&& reply.is_string()) {
			//
			cb(std::move(reply.as_string()));
		}
		else {
			cb(std::string(""));
		}
	};
	redisservice.HGet(_sIdHash, sId);
	redisservice.Commit(rcb);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::GetAll(RESULT_MAP& mapOut) {
	IRedisService& redisservice = _refRedisHandler->RedisService();

	redisservice.HGetAll(_sIdHash);

	CRedisReply reply = redisservice.BlockingCommit();
	if (reply.ok()
		&& !reply.is_null()
		&& reply.is_array()) {
		//
		const std::vector<CRedisReply>& v = reply.as_array();
		int nCount = v.size() / 2;

		// save in map
		int i;
		for (i = 0; i < nCount; ++i) {
			const CRedisReply& r1 = v[i * 2];
			const CRedisReply& r2 = v[i * 2 + 1];

			if (r1.ok() && r2.ok()
				&& !r1.is_null() && !r2.is_null()
				&& (r1.is_string() || r1.is_bulk_string()) && (r2.is_string() || r2.is_bulk_string())) {
				//
				const std::string& rkey = r1.as_string();
				const std::string& rval = r2.as_string();

				mapOut[std::move(rkey)] = std::move(rval);
			}
		}
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::GetAll(on_got_all cb) {
	IRedisService& redisservice = _refRedisHandler->RedisService();
	IRedisService::reply_cb_t rcb = [&cb](CRedisReply& reply) {

		RESULT_MAP mapTmp;

		if (reply.ok()
			&& !reply.is_null()
			&& reply.is_array()) {
			//
			const std::vector<CRedisReply>& v = reply.as_array();
			int nCount = v.size() / 2;

			// save in map
			int i;
			for (i = 0; i < nCount; ++i) {
				const CRedisReply& r1 = v[i * 2];
				const CRedisReply& r2 = v[i * 2 + 1];

				if (r1.ok() && r2.ok()
					&& !r1.is_null() && !r2.is_null()
					&& (r1.is_string() || r1.is_bulk_string()) && (r2.is_string() || r2.is_bulk_string())) {
					//
					const std::string& rkey = r1.as_string();
					const std::string& rval = r2.as_string();

					mapTmp[std::move(rkey)] = std::move(rval);
				}
			}
		}

		//
		cb(mapTmp);
	};
	redisservice.HGetAll(_sIdHash);
	redisservice.Commit(rcb);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisCacheProxy::LootDirtyIds(std::vector<std::string>& vOut) {
	IRedisService& redisservice = _refRedisHandler->RedisService();
	redis_stub_param_t& rs_param = _refRedisHandler->RedisStubParam();
	std::string& sDirtyEntry = rs_param._dirtyEntry;

	redisservice.Watch(_sIdSetOfDirty);
	redisservice.Multi();
	{
		redisservice.SMembers(_sIdSetOfDirty);
		redisservice.Del(std::vector<std::string>{ _sIdSetOfDirty });
		redisservice.HDel(sDirtyEntry, std::vector<std::string>{ _sCacheName });
	}
	redisservice.Exec();

	CRedisReply reply = redisservice.BlockingCommit();
	if (reply.ok()
		&& !reply.is_null()
		&& reply.is_array()) {
		//
		const std::vector<CRedisReply>& vReply = reply.as_array();
		const CRedisReply& r1 = vReply[0];
		const CRedisReply& r2 = vReply[1];
		const CRedisReply& r3 = vReply[2];

		if (r1.ok() && !r1.is_null() && r1.is_array() && r2.ok() && r3.ok()) {
			//
			const std::vector<CRedisReply>& v1 = r1.as_array();
			for (auto& rr1 : v1) {
				if (rr1.ok()
					&& !rr1.is_null()
					&& (rr1.is_string() || rr1.is_bulk_string())) {

					vOut.push_back(std::move(rr1.as_string()));
				}
			}
		}
	}
}

/* -- EOF -- */