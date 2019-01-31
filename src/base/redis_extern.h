#ifndef __REDIS_EXTERN_H__
#define __REDIS_EXTERN_H__

/* Export functions from the DLL */
#ifndef MY_REDIS_EXTERN
# if defined(WIN32) || defined(_WIN32)
/* Windows - set up dll import/export decorators. */
#  if defined(MY_REDIS_BUILDING_SHARED)
/* Building shared library. */
#   define MY_REDIS_EXTERN __declspec(dllexport)
#  elif defined(MY_REDIS_USING_SHARED)
/* Using shared library. */
#   define MY_REDIS_EXTERN __declspec(dllimport)
#  else
/* Building static library. */
#    define MY_REDIS_EXTERN /* nothing */
#  endif
# elif __GNUC__ >= 4
#  define MY_REDIS_EXTERN __attribute__((visibility("default")))
# else
#  define MY_REDIS_EXTERN /* nothing */
# endif
#endif

#endif /* __REDIS_EXTERN_H__ */