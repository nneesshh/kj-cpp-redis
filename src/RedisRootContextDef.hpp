#ifndef _REDIS_ROOT_CONTEXT_DEF_HPP__
#define _REDIS_ROOT_CONTEXT_DEF_HPP__

//------------------------------------------------------------------------------
/**
	@class CRedisRootContextDef

	(C) 2016 n.lee
*/
#include "servercore/base/IServerCore.h"

extern "C" void redis_init_servercore(void *servercore);
extern "C" void redis_cleanup_servercore();
extern "C" IServerCore * redis_get_servercore();

extern "C" StdLog * redis_get_log();

#endif /* _REDIS_ROOT_CONTEXT_DEF_HPP__ */