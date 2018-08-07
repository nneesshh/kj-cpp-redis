//------------------------------------------------------------------------------
//  RedisService.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"

#include "RedisClient.h"
#include "RedisSubscriber.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

static kj::Own<KjSimpleIoContext> s_rootContext = kj::refcounted<KjSimpleIoContext>();

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
CRedisService::CRedisService(redis_stub_param_t *param)
	: _param(*param) {
	//
	_rp = create_rdb_parser();

	//
	_redisClient = new CRedisClient(kj::addRef(*s_rootContext), _param);
	_redisSubscriber = new CRedisSubscriber(kj::addRef(*s_rootContext), _param);
}

//------------------------------------------------------------------------------
/**

*/
CRedisService::~CRedisService() {

	delete _redisClient;
	delete _redisSubscriber;

	destroy_rdb_parser(_rp);

	//
	s_rootContext = nullptr;
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