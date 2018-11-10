#pragma once

#include "rdb.h"

#include "../bip_buf.h"
#include "../nx_buf.h"
#include "../nx_array.h"

/* kv */
typedef struct rdb_kv_s        rdb_kv_t;
typedef struct rdb_kv_chain_s  rdb_kv_chain_t;

struct rdb_kv_s {
    nx_str_t                    key;
    nx_str_t                    val;
};

struct rdb_kv_chain_s {
    rdb_kv_t                   *kv;
    rdb_kv_chain_t             *next;
};

/* object */
typedef struct rdb_object_s        rdb_object_t;
typedef struct rdb_object_chain_s  rdb_object_chain_t;

struct rdb_object_s {
    uint8_t                     type;

    uint32_t                    db_selector;
    nx_str_t                    aux_key;
	nx_str_t                    aux_val;
    uint64_t                    checksum;
    int                         expire;

    nx_str_t                    key;
    nx_str_t                    val;
    rdb_kv_chain_t             *vall;
    rdb_kv_chain_t             *vall_tail;
    size_t                      size;

};

struct rdb_object_chain_s {
    rdb_object_t                 *elem;
    rdb_object_chain_t           *next;
};

/* parser */
typedef struct rdb_object_builder_s  rdb_object_builder_t;
typedef struct rdb_parser_s          rdb_parser_t;

typedef int(*func_walk_rdb_object)(rdb_object_t *o, void *payload);

struct rdb_object_builder_s {
    uint8_t                     depth;
    uint8_t                     state;

    /* for string, list, etc. */
    uint32_t                    store_len;
    uint32_t                    c_len; /* compress len */
    uint32_t                    len;

    nx_str_t                    tmp_key;
    nx_str_t                    tmp_val;

};

struct rdb_parser_s {
    int                         version;
    uint64_t                    chksum;
    uint64_t                    parsed;
    uint8_t                     state;

    bip_buf_t                  *in_bb;
	nx_pool_t                  *pool;

	nx_array_t                 *stack_ob;

	rdb_object_t               *o;

	nx_pool_t                  *o_pool;
	func_walk_rdb_object     o_cb;
	void                       *o_payload;
};

#define rdb_object_init(_o)	   nx_memzero(_o, sizeof(rdb_object_t))
#define rdb_object_clear(_rp)  rdb_object_init(_rp->o);nx_reset_pool(_rp->o_pool)

/* EOF */