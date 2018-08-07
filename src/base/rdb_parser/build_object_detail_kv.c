#include "build_factory.h"

enum BUILD_OBJECT_DETAIL_KV_TAG {
    BUILD_OBJECT_DETAIL_KV_IDLE = 0,
    BUILD_OBJECT_DETAIL_KV_KEY,
    BUILD_OBJECT_DETAIL_KV_VAL,
};

static int
__build_object_detail_kv_key(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    int rc = 0;

    rdb_object_t *o;

	o = rp->o;

    /* key */
    rc = build_string_value(rp, ob, bb, &o->key);

    if (rc == OB_OVER) {
        /* next state */
        ob->state = BUILD_OBJECT_DETAIL_KV_VAL;
        return OB_AGAIN;
    }
    return rc;
}

int
build_object_detail_kv(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    int rc = 0;
    rdb_object_builder_t *sub_ob;
    int depth = ob->depth + 1;

    /* sub builder */
    OB_LOOP_BEGIN(rp, sub_ob, depth)
    {
        /* sub process */
        switch (sub_ob->state) {
        case BUILD_OBJECT_DETAIL_KV_IDLE:
        case BUILD_OBJECT_DETAIL_KV_KEY:
            rc = __build_object_detail_kv_key(rp, sub_ob, bb);
            break;

        case BUILD_OBJECT_DETAIL_KV_VAL:
            rc = build_object_detail_kv_val(rp, sub_ob, bb);
            break;

        default:
            rc = OB_ERROR_INVALID_NB_STATE;
            break;
        }

    }
    OB_LOOP_END(rp, rc)
    return rc;
}