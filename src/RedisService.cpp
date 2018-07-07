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

//------------------------------------------------------------------------------
/**

*/
CRedisService::CRedisService(redis_stub_param_t *param)
	: _param(*param) {
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

	//
	s_rootContext = nullptr;
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