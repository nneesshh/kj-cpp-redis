#include "build_factory.h"

int
build_object_detail_kv_val(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    int rc = 0;

    rdb_object_t *o;

	o = rp->o;

    switch (o->type) {
    case RDB_TYPE_STRING:
        /* string */
        return build_string_value(rp, ob, bb, &o->val);

    case RDB_TYPE_LIST:
    case RDB_TYPE_SET:
        return build_list_or_set_value(rp, ob, bb, &o->vall, &o->size);

    case RDB_TYPE_ZSET:
    case RDB_TYPE_HASH:
        return build_hash_or_zset_value(rp, ob, bb, &o->vall, &o->size);

    case RDB_TYPE_HASH_ZIPMAP:
        return build_zipmap_value(rp, ob, bb, &o->vall, &o->size);

    case RDB_TYPE_LIST_ZIPLIST:
        return build_zl_list_value(rp, ob, bb, &o->vall, &o->size);

    case RDB_TYPE_SET_INTSET:
        return build_intset_value(rp, ob, bb, &o->vall, &o->size);

    case RDB_TYPE_ZSET_ZIPLIST:
    case RDB_TYPE_HASH_ZIPLIST:
        return build_zl_hash_value(rp, ob, bb, &o->vall, &o->size);

    default:
        break;
    }
    return OB_ERROR_INVALID_NB_TYPE;
}
