//------------------------------------------------------------------------------
//  RedisServicePlugin.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"

#include "base/redis_extern.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

extern "C" {
	MY_REDIS_EXTERN IRedisService *
		GetPlugin(void *servercore, redis_stub_param_t *param) {
		return new CRedisService(servercore, param);
	}

	MY_REDIS_EXTERN IRedisService *
		GetClass(void *servercore, redis_stub_param_t *param) {
		return GetPlugin(servercore, param);
	}
}

/** -- EOF -- **/