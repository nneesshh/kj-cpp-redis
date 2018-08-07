#include "build_factory.h"

#include "../endian.h"
#include "rdb_parser.h"

enum BUILD_BODY_TAG {
    BUILD_BODY_IDLE = 0,
    BUILD_BODY_OBJECT_TYPE,
    BUILD_BODY_OBJECT_DETAIL,
};

static size_t
__read_db_selector(rdb_parser_t *rp, bip_buf_t *bb, uint32_t *out)
{
    size_t n;
    uint32_t selector;

    if ((n = rdb_object_read_store_len(rp, bb, NULL, &selector)) == 0)
        return 0;

    (*out) = selector;
    return n;
}

static size_t
__read_expire_time(rdb_parser_t *rp, bip_buf_t *bb, int is_ms, int *out)
{
    size_t bytes = 1;
    uint8_t *ptr;
    uint32_t t32;
    uint64_t t64;

    if (is_ms) {
        /* milliseconds */
        bytes = 8;

        if (bip_buf_get_committed_size(bb) < bytes) {
            return 0;
        }

        ptr = bip_buf_get_contiguous_block(bb);
        t64 = *(uint64_t *)ptr;
        memrev64(&t64);
        (*out) = (int)(t64 / 1000);
    }
    else {
        /* seconds */
        bytes = 4;

        if (bip_buf_get_committed_size(bb) < bytes) {
            return 0;
        }

        ptr = bip_buf_get_contiguous_block(bb);
        t32 = *(uint32_t *)ptr;
        memrev32(&t32);
        (*out) = (int)t32;
    }

    return bytes;
}

static int __build_object_type(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb);
static int __build_object_detail_kv_val(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb);

static int
__build_object_type(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    size_t n;
    uint8_t type;

    rdb_object_t *o;
    
    if ((n = rdb_object_read_type(rp, bb, &type)) == 0)
        return OB_ERROR_PREMATURE;

    o = rp->o;
    o->type = type;

    /* ok */
    rdb_object_calc_crc(rp, bb, n);

    /* next state */
    ob->state = BUILD_BODY_OBJECT_DETAIL;
    return OB_AGAIN;
}

static int
__build_object_detail_kv_val(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    int rc = 0;
    size_t n;
    int is_ms;

    rdb_object_t *o;

	o = rp->o;
    
    switch (o->type) {
    case RDB_OPCODE_AUX:
        /* aux fields */
        rc = build_opcode_aux(rp, ob, bb);

        if (rc == OB_OVER) {
            /* process reply */
            if (0 == rp->o_cb(rp->o, rp->o_payload)) {
                rdb_object_clear(rp);

                /* next state */
                ob->state = BUILD_BODY_OBJECT_TYPE;
                return OB_AGAIN;
            }
            else {
                rc = OB_ABORT;
            }
        }
        return rc;

    case RDB_OPCODE_SELECTDB:
        /* db selector */
        if ((n = __read_db_selector(rp, bb,  &o->db_selector)) == 0) {
            return OB_ERROR_PREMATURE;
        }

        /* ok */
        rdb_object_calc_crc(rp, bb, n);
        
        /* process reply */
        if (0 == rp->o_cb(rp->o, rp->o_payload)) {
            rdb_object_clear(rp);

            /* next state */
            ob->state = BUILD_BODY_OBJECT_TYPE;
            return OB_AGAIN;
        }
        else {
            rc = OB_ABORT;
        }
        return rc;

    case RDB_OPCODE_EXPIRETIME:
    case RDB_OPCODE_EXPIRETIME_MS:
        /* expire time */
        is_ms = (RDB_OPCODE_EXPIRETIME_MS == o->type);;
        if ((n = __read_expire_time(rp, bb, is_ms, &o->expire)) == 0) {
            return OB_ERROR_PREMATURE;
        }

        /* ok */
        rdb_object_calc_crc(rp, bb, n);

		/* process reply */
		if (0 == rp->o_cb(rp->o, rp->o_payload)) {
			rdb_object_clear(rp);

			/* next state */
			ob->state = BUILD_BODY_OBJECT_TYPE;
			return OB_AGAIN;
		}
		else {
			rc = OB_ABORT;
		}
		return rc;

    case RDB_TYPE_STRING:
    case RDB_TYPE_LIST:
    case RDB_TYPE_SET:
    case RDB_TYPE_ZSET:
    case RDB_TYPE_HASH:
    case RDB_TYPE_HASH_ZIPMAP:
    case RDB_TYPE_LIST_ZIPLIST:
    case RDB_TYPE_SET_INTSET:
    case RDB_TYPE_ZSET_ZIPLIST:
    case RDB_TYPE_HASH_ZIPLIST: {
        /* object detail kv */
        rc = build_object_detail_kv(rp, ob, bb);

        if (rc == OB_OVER) {
			/* process reply */
			if (0 == rp->o_cb(rp->o, rp->o_payload)) {
				rdb_object_clear(rp);

				/* next state */
				ob->state = BUILD_BODY_OBJECT_TYPE;
				return OB_AGAIN;
			}
			else {
				rc = OB_ABORT;
			}
        }
        return rc;
    }

    case RDB_OPCODE_EOF: {
        return OB_OVER;
    }

    default:
        return OB_ERROR_INVALID_NB_TYPE;
    }

    /* premature because val is not completed yet */
    return OB_ERROR_PREMATURE;
}

int
build_body(rdb_parser_t *rp, bip_buf_t *bb)
{
    int rc = 0;
    rdb_object_builder_t *ob;
    int depth = 0;

    /* main builder */
    OB_LOOP_BEGIN(rp, ob, depth)
    {
        /* main process */
        switch (ob->state) {
        case BUILD_BODY_IDLE:
        case BUILD_BODY_OBJECT_TYPE:
            rc = __build_object_type(rp, ob, bb);
            break;

        case BUILD_BODY_OBJECT_DETAIL:
            rc = __build_object_detail_kv_val(rp, ob, bb);
            break;

        default:
            rc = OB_ERROR_INVALID_NB_STATE;
            break;
        }

    }
    OB_LOOP_END(rp, rc)
    return rc;
}

