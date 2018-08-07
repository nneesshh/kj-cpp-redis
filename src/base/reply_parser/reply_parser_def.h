#pragma once

#include "../bip_buf.h"
#include "../nx_buf.h"
#include "../nx_array.h"

/* leading type */
enum REDIS_REPLY_LEADING_TYPE_TAG {
    REDIS_REPLY_LEADING_TYPE_UNKNOWN = 0,
    REDIS_REPLY_LEADING_TYPE_SIMPLE_STRING,
    REDIS_REPLY_LEADING_TYPE_ERROR_STRING,
    REDIS_REPLY_LEADING_TYPE_INTEGER,
    REDIS_REPLY_LEADING_TYPE_BULK_STRING,
    REDIS_REPLY_LEADING_TYPE_ARRAY,
};

/* reply */
typedef struct redis_reply_s        redis_reply_t;
typedef struct redis_reply_chain_s  redis_reply_chain_t;

struct redis_reply_s {
    uint8_t                     type;
    uint8_t                     is_null;

    nx_chain_t                 *vall;
    nx_chain_t                 *vall_tail;
    size_t                      bytes;

    int64_t                     intval;

    nx_array_t                 *arrval;
    uint8_t                     elemtype;
};

struct redis_reply_chain_s {
    redis_reply_t              *elem;
    redis_reply_chain_t        *next;
};

/* parser */
typedef struct redis_reply_builder_s  redis_reply_builder_t;
typedef struct redis_reply_parser_s   redis_reply_parser_t;

typedef int(*func_process_redis_reply)(redis_reply_t *r, void *payload);

struct redis_reply_builder_s {
    uint8_t                     depth;
    uint8_t                     state;

    /* for string, list, etc. */
    uint32_t                    store_len;
    uint32_t                    len;
    int                         arridx;
};

struct redis_reply_parser_s {
    int                         version;
    uint64_t                    parsed;
    uint8_t                     state;

    bip_buf_t                  *in_bb;
    nx_pool_t                  *pool;

    nx_array_t                 *stack_rrb;

	redis_reply_t              *r;

	nx_pool_t                  *r_pool;
    func_process_redis_reply    r_cb;
    void                       *r_payload;
};

#define redis_reply_init(_r)	 nx_memzero(_r, sizeof(redis_reply_t))
#define redis_reply_clear(_rrp)  redis_reply_init(_rrp->r);nx_reset_pool(_rrp->r_pool)

/* EOF */