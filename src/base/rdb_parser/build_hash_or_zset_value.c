#include "build_factory.h"

enum BUILD_HASH_OR_ZSET_TAG {
    BUILD_HASH_OR_ZSET_IDLE = 0,
    BUILD_HASH_OR_ZSET_STORE_LEN,
    BUILD_HASH_OR_ZSET_LOOP_KEY,
    BUILD_HASH_OR_ZSET_LOOP_VAL,
};

static int __build_hash_or_zset_store_len(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb);
static int __build_hash_or_zset_loop_key(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb);
static int __build_hash_or_zset_loop_val(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, rdb_kv_chain_t **vall, size_t *size);

static int
__build_hash_or_zset_store_len(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    size_t n;
    uint32_t len;

    if ((n = rdb_object_read_store_len(rp, bb, NULL, &len)) == 0)
        return OB_ERROR_PREMATURE;

    ob->store_len = len;
    ob->len = 0;

    /* ok */
    rdb_object_calc_crc(rp, bb, n);

    /* next state */
    ob->state = BUILD_HASH_OR_ZSET_LOOP_KEY;
    return OB_AGAIN;
}

static int
__build_hash_or_zset_loop_key(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    int rc = 0;

    if (ob->len < ob->store_len) {
        rc = build_string_value(rp, ob, bb, &ob->tmp_key);

        if (rc == OB_OVER) {
            /* next state */
            ob->state = BUILD_HASH_OR_ZSET_LOOP_VAL;
            return OB_AGAIN;
        }
        return rc;
    }
    else {
        /* next state */
        ob->state = BUILD_HASH_OR_ZSET_LOOP_VAL;
        return OB_AGAIN;
    }
}

static int
__build_hash_or_zset_loop_val(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, rdb_kv_chain_t **vall, size_t *size)
{
    int rc = 0;

    rdb_object_t *o;
    rdb_kv_chain_t **ll;

	o = rp->o;

    if (ob->len < ob->store_len) {
        rc = build_string_value(rp, ob, bb, &ob->tmp_val);

        /* over */
        if (rc == OB_OVER) {

            ll = (NULL == o->vall_tail) ? vall : &o->vall_tail;
            o->vall_tail = alloc_rdb_kv_chain_link(rp->o_pool, ll);

            nx_str_set2(&o->vall_tail->kv->key, ob->tmp_key.data, ob->tmp_key.len);
            nx_str_set2(&o->vall_tail->kv->val, ob->tmp_val.data, ob->tmp_val.len);

            nx_str_null(&ob->tmp_key);
            nx_str_null(&ob->tmp_val);

            ++ob->len;

            if (ob->len < ob->store_len) {
                /* next state */
                ob->state = BUILD_HASH_OR_ZSET_LOOP_KEY;
                return OB_AGAIN;
            }

            (*size) = ob->len;
        }
        return rc;
    }
    else {
        /* next state */
        ob->state = BUILD_HASH_OR_ZSET_LOOP_VAL;
        return OB_AGAIN;
    }
}

int
build_hash_or_zset_value(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, rdb_kv_chain_t **vall, size_t *size)
{
    int rc = 0;
    rdb_object_builder_t *sub_ob;
    int depth = ob->depth + 1;

    /* sub builder */
    OB_LOOP_BEGIN(rp, sub_ob, depth)
    {
        /* sub process */
        switch (sub_ob->state) {
        case BUILD_HASH_OR_ZSET_IDLE:
        case BUILD_HASH_OR_ZSET_STORE_LEN:
            rc = __build_hash_or_zset_store_len(rp, sub_ob, bb);
            break;

        case BUILD_HASH_OR_ZSET_LOOP_KEY:
            rc = __build_hash_or_zset_loop_key(rp, sub_ob, bb);
            break;

        case BUILD_HASH_OR_ZSET_LOOP_VAL:
            rc = __build_hash_or_zset_loop_val(rp, sub_ob, bb, vall, size);
            break;

        default:
            rc = OB_ERROR_INVALID_NB_STATE;
            break;
        }

    }
    OB_LOOP_END(rp, rc)
    return rc;
}
