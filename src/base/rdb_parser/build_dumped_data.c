#include "build_factory.h"

#include "../endian.h"
#include "rdb_parser.h"

enum BUILD_DUMPED_DATA_TAG {
    BUILD_DUMPED_DATA_IDLE = 0,
    BUILD_DUMPED_DATA_OBJECT_TYPE,
    BUILD_DUMPED_DATA_OBJECT_DETAIL_KV_VAL,
};

static int
__build_dumped_data_object_type(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
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
    ob->state = BUILD_DUMPED_DATA_OBJECT_DETAIL_KV_VAL;
    return OB_AGAIN;
}

int
build_dumped_data(rdb_parser_t *rp, bip_buf_t *bb)
{
    int rc = 0;
    rdb_object_builder_t *ob;
    int depth = 0;

    /* main builder */
    OB_LOOP_BEGIN(rp, ob, depth)
    {
        /* main process */
        switch (ob->state) {
        case BUILD_DUMPED_DATA_IDLE:
        case BUILD_DUMPED_DATA_OBJECT_TYPE:
            rc = __build_dumped_data_object_type(rp, ob, bb);
            break;

        case BUILD_DUMPED_DATA_OBJECT_DETAIL_KV_VAL:
            rc = build_object_detail_kv_val(rp, ob, bb);

			if (rc == OB_OVER) {
				/* process reply */
				if (0 == rp->o_cb(rp->o, rp->o_payload)) {
					rdb_object_clear(rp);
				}
				else {
					rc = OB_ABORT;
				}
			}
            break;

        default:
            rc = OB_ERROR_INVALID_NB_STATE;
            break;
        }

    }
    OB_LOOP_END(rp, rc)
    return rc;
}

