//------------------------------------------------------------------------------
//  RedisService.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"

#include "RedisRootContextDef.hpp"

#include "RedisClient.h"
#include "RedisSubscriber.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

typedef std::function<int(rdb_object_t *r)> __parse_dumped_data_cb;

struct __parse_dumped_data_host_t {
	__parse_dumped_data_cb _cb;
};

static int
__on_got_rdb_object(rdb_object_t *r, void *payload) {
	__parse_dumped_data_host_t *host = static_cast<struct __parse_dumped_data_host_t *>(payload);
	return host->_cb(r);
}

//------------------------------------------------------------------------------
/**

*/
CRedisService::CRedisService(void *servercore, redis_stub_param_t *param)
	: _param(*param) {
	//
	_rp = create_rdb_parser();

	//
	redis_init_servercore(servercore);

	//
	_redisClient = new CRedisClient(_param);
	_redisSubscriber = new CRedisSubscriber(_param);
}

//------------------------------------------------------------------------------
/**

*/
CRedisService::~CRedisService() noexcept {

	delete _redisClient;
	delete _redisSubscriber;

	destroy_rdb_parser(_rp);

	//
	redis_cleanup_servercore();
}

//------------------------------------------------------------------------------
/**

*/
int
CRedisService::ParseDumpedData(const std::string& sDump, std::function<int(rdb_object_t *)>&& cb) {
	struct __parse_dumped_data_host_t host;
	host._cb = std::move(cb);
	return rdb_parse_dumped_data(_rp, __on_got_rdb_object, &host, sDump.c_str(), sDump.length());
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisService::Shutdown() {
	if (!_bShutdown) {
		_bShutdown = true;

		_redisClient->Shutdown();
		_redisSubscriber->Shutdown();
	}
}

/** -- EOF -- **/