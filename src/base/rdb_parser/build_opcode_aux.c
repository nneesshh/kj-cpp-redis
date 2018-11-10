#include "build_factory.h"

enum BUILD_OPCODE_AUX_TAG {
    BUILD_OPCODE_AUX_IDLE = 0,
	BUILD_OPCODE_AUX_KEY,
    BUILD_OPCODE_AUX_VAL,
};

int
build_opcode_aux(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    int rc = 0;
    rdb_object_builder_t *sub_ob;
    int depth = ob->depth + 1;

	rdb_object_t *o;

	o = rp->o;

    /* main builder */
    OB_LOOP_BEGIN(rp, sub_ob, depth)
    {
        /* main process */
        switch (sub_ob->state) {
        case BUILD_OPCODE_AUX_IDLE:
        case BUILD_OPCODE_AUX_KEY:
            rc = build_string_value(rp, sub_ob, bb, &o->aux_key);

			if (rc == OB_OVER) {
				/* next state */
				ob->state = BUILD_OPCODE_AUX_VAL;
				return OB_AGAIN;

			}
            break;

        case BUILD_OPCODE_AUX_VAL:
			rc = build_string_value(rp, sub_ob, bb, &o->aux_val);
            break;

        default:
            rc = OB_ERROR_INVALID_NB_STATE;
            break;
        }

    }
    OB_LOOP_END(rp, rc)
    return rc;
}
