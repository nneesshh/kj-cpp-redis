#pragma once

#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <malloc.h>


#ifdef _MSC_VER /* msvc */
# pragma warning(disable : 4786)
# ifndef INLINE
# define INLINE __inline
# endif
# ifndef NOINLINE
# define NOINLINE __declspec (noinline)
# endif
#else  /* gcc */
# ifndef INLINE
# define INLINE inline
# endif
# ifndef NOINLINE
# define NOINLINE __attribute__ ((noinline))
# endif
#endif


typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned int    u_int;
typedef unsigned long   u_long;


#define  NX_OK          0
#define  NX_ERROR      -1
#define  NX_AGAIN      -2
#define  NX_BUSY       -3
#define  NX_DONE       -4
#define  NX_DECLINED   -5
#define  NX_ABORT      -6


typedef struct {
    size_t      len;
    u_char     *data;
} nx_str_t;


typedef struct {
    nx_str_t   key;
    nx_str_t   value;
} nx_keyval_t;


#define nx_string(str)     { sizeof(str) - 1, (u_char *) str }
#define nx_null_string     { 0, NULL }
#define nx_str_set2(str, text, text_len)                                       \
    (str)->data = (text); (str)->len = (text_len)
#define nx_str_null(str)   (str)->len = 0; (str)->data = NULL


#define nx_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define nx_toupper(c)      (u_char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

#define nx_strncmp(s1, s2, n)  strncmp((const char *) s1, (const char *) s2, n)

/* msvc and icc7 compile strcmp() to inline loop */
#define nx_strcmp(s1, s2)  strcmp((const char *) s1, (const char *) s2)
#define nx_strstr(s1, s2)  strstr((const char *) s1, (const char *) s2)
#define nx_strlen(s)       strlen((const char *) s)
#define nx_strchr(s1, c)   strchr((const char *) s1, (int) c)

INLINE u_char *
nx_strlchr(u_char *p, u_char *last, u_char c)
{
    while (p < last) {

        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}


/*
* msvc and icc7 compile memset() to the inline "rep stos"
* while ZeroMemory() and bzero() are the calls.
* icc7 may also inline several mov's of a zeroed register for small blocks.
*/
#define nx_memzero(buf, n)       (void) memset(buf, 0, n)
#define nx_memset(buf, c, n)     (void) memset(buf, c, n)

/*
* gcc3, msvc, and icc7 compile memcpy() to the inline "rep movs".
* gcc3 compiles memcpy(d, s, 4) to the inline "mov"es.
* icc8 compile memcpy(d, s, 4) to the inline "mov"es or XMM moves.
*/
#define nx_memcpy(dst, src, n)   (void) memcpy(dst, src, n)
#define nx_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))

#ifndef NX_ALIGNMENT
#define NX_ALIGNMENT  sizeof(unsigned long)    /* platform word */
#endif


#define nx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define nx_align_ptr(p, a)                                                     \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))  


#define nx_free          free
#define nx_memalign(alignment, size)  nx_alloc(size)


#define nx_buf_size(b) (size_t) (b->last - b->pos)


/*
* NX_MAX_ALLOC_FROM_POOL should be (nx_pagesize - 1), i.e. 4095 on x86.
* On Windows NT it decreases a number of locked pages in a kernel.
*/
#define nx_pagesize             4096
#define NX_MAX_ALLOC_FROM_POOL  (nx_pagesize - 1)

#define NX_DEFAULT_POOL_SIZE    (16 * 1024)

#define NX_POOL_ALIGNMENT       16
#define NX_MIN_POOL_SIZE                                                       \
    nx_align((sizeof(nx_pool_t) + 2 * sizeof(nx_pool_large_t)),                \
              NX_POOL_ALIGNMENT)


#define nx_alloc_buf(pool)  nx_palloc(pool, sizeof(nx_buf_t))
#define nx_calloc_buf(pool) nx_pcalloc(pool, sizeof(nx_buf_t))

#define nx_free_chain(pool, cl)                                                \
    cl->next = pool->chain;                                                    \
    pool->chain = cl


#ifdef __cplusplus 
extern "C" {
#endif 


typedef struct nx_buf_s  nx_buf_t;

struct nx_buf_s {
    u_char             *pos;
    u_char             *last;

    u_char             *start;         /* start of buffer */
    u_char             *end;           /* end of buffer */
};


typedef void(*nx_pool_cleanup_pt)(void *data);
typedef struct nx_pool_cleanup_s  nx_pool_cleanup_t;

struct nx_pool_cleanup_s {
    nx_pool_cleanup_pt  handler;
    void               *data;
    nx_pool_cleanup_t  *next;
};


typedef struct nx_pool_large_s  nx_pool_large_t;

struct nx_pool_large_s {
    nx_pool_large_t    *next;
    void               *alloc;
};


typedef struct nx_pool_s   nx_pool_t;
typedef struct nx_chain_s  nx_chain_t;

typedef struct {
    u_char             *last;
    u_char             *end;
    nx_pool_t          *next;
    u_int               failed;
} nx_pool_data_t;

struct nx_pool_s {
    nx_pool_data_t      d;
    size_t              max;
    nx_pool_t          *current;
    nx_chain_t         *chain;
    nx_pool_large_t    *large;
    nx_pool_cleanup_t  *cleanup;
};

struct nx_chain_s {
    nx_buf_t           *buf;
    nx_chain_t         *next;
};


typedef struct {
    int                 num;
    size_t              size;
} nx_bufs_t;


/* alloc */
void               *nx_alloc(size_t size);
void               *nx_calloc(size_t size);


/* pool */
nx_pool_t          *nx_create_pool(size_t size);
void                nx_destroy_pool(nx_pool_t *pool);
void                nx_reset_pool(nx_pool_t *pool);

void               *nx_palloc(nx_pool_t *pool, size_t size);
void               *nx_pnalloc(nx_pool_t *pool, size_t size);
void               *nx_pcalloc(nx_pool_t *pool, size_t size);
void               *nx_pmemalign(nx_pool_t *pool, size_t size, size_t alignment);
int                 nx_pfree(nx_pool_t *pool, void *p);

nx_pool_cleanup_t  *nx_pool_cleanup_add(nx_pool_t *p, size_t size);


/* buf and chain */
nx_buf_t           *nx_create_temp_buf(nx_pool_t *pool, size_t size);
nx_chain_t         *nx_create_chain_of_bufs(nx_pool_t *pool, nx_bufs_t *bufs);
nx_chain_t         *nx_alloc_chain_link(nx_pool_t *pool);

int                 nx_chain_add_copy(nx_pool_t *pool, nx_chain_t **chain, nx_chain_t *in);
nx_chain_t         *nx_chain_get_free_buf(nx_pool_t *p, nx_chain_t **free);
void                nx_chain_update_chains(nx_pool_t *p, nx_chain_t **free, nx_chain_t **busy, nx_chain_t **out);

nx_chain_t         *nx_chain_update_sent(nx_chain_t *in, u_long sent);


#ifdef __cplusplus 
}
#endif 

/*EOF*/