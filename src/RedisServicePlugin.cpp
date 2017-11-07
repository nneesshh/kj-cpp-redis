//------------------------------------------------------------------------------
//  RedisServicePlugin.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisService.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

/* Export functions from the DLL */
#ifndef MY_EXTERN
# if defined(WIN32) || defined(_WIN32)
/* Windows - set up dll import/export decorators. */
#  if defined(MY_BUILDING_SHARED)
/* Building shared library. */
#   define MY_EXTERN __declspec(dllexport)
#  elif defined(MY_USING_SHARED)
/* Using shared library. */
#   define MY_EXTERN __declspec(dllimport)
#  else
/* Building static library. */
#    define MY_EXTERN /* nothing */
#  endif
# elif __GNUC__ >= 4
#  define MY_EXTERN __attribute__((visibility("default")))
# else
#  define MY_EXTERN /* nothing */
# endif
#endif

extern "C" {
	MY_EXTERN IRedisService *
		GetPlugin(redis_stub_param_t *param) {
		return new CRedisService(param);
	}

	MY_EXTERN IRedisService *
		GetClass(redis_stub_param_t *param) {
		return GetPlugin(param);
	}
}

/** -- EOF -- **/