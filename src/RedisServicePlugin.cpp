//------------------------------------------------------------------------------
//  RedisServicePlugin.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"

#include "base/platform_types.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

extern "C" {
	MY_REDIS_EXTERN IRedisService *
		GetPlugin(redis_stub_param_t *param) {
		return new CRedisService(param);
	}

	MY_REDIS_EXTERN IRedisService *
		GetClass(redis_stub_param_t *param) {
		return GetPlugin(param);
	}
}

/** -- EOF -- **/