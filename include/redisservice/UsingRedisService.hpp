#pragma once
//------------------------------------------------------------------------------
/**
@class CUsingRedisService

(C) 2016 n.lee
*/
#ifdef _MSC_VER
#pragma warning(disable:4251)
#pragma warning(disable:4250)
#endif

#ifdef _WIN32
#pragma comment(lib, "WS2_32.Lib")
#endif

#include "base/redis_service_def.h"
#include "base/RedisReply.h"
#include "base/RedisCacheProxy.h"
#include "base/RedisListProxy.h"
#include "base/RedisRankingProxy.h"

#include "RedisService.h"

/*EOF*/