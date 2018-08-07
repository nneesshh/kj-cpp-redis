#include "build_factory.h"

#include <stdio.h>
#include <string.h>

#include "lzf.h"

enum BUILD_LZF_STRING_TAG {
    BUILD_LZF_STRING_IDLE = 0,
    BUILD_LZF_STRING_COMPRESS_LEN,
    BUILD_LZF_STRING_RAW_LEN,
    BUILD_LZF_STRING_LZF,
};

static int __build_lzf_string_compress_len(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb);
static int __build_lzf_string_raw_len(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb);
static int __build_lzf_string_lzf(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, nx_str_t *val);

static int
__build_lzf_string_compress_len(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    size_t n;

    if ((n = rdb_object_read_store_len(rp, bb, NULL, &ob->c_len)) == 0)
        return OB_ERROR_PREMATURE;

    /* ok */
    rdb_object_calc_crc(rp, bb, n);

    /* next state */
    ob->state = BUILD_LZF_STRING_RAW_LEN;

    /* pre-alloc */
    ob->tmp_val.data = nx_palloc(rp->o_pool, ob->c_len + 1);
    ob->len = 0;
    return OB_AGAIN;
}

static int
__build_lzf_string_raw_len(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb)
{
    size_t n;

    if ((n = rdb_object_read_store_len(rp, bb, NULL, &ob->len)) == 0)
        return OB_ERROR_PREMATURE;

    /* ok */
    rdb_object_calc_crc(rp, bb, n);

    /* next state */
    ob->state = BUILD_LZF_STRING_LZF;
    return OB_AGAIN;
}

static int
__build_lzf_string_lzf(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, nx_str_t *val)
{
    size_t buf_size, want_size, consume_size;
    uint8_t *ptr;
    char *s, *raw;
    int ret;

    buf_size = bip_buf_get_committed_size(bb);
    want_size = ob->c_len - ob->tmp_val.len;
    consume_size = buf_size > want_size ? want_size : buf_size;

    s = (char *)ob->tmp_val.data + ob->tmp_val.len;
    ptr = bip_buf_get_contiguous_block(bb);
    nx_memcpy(s, ptr, consume_size);
    ob->tmp_val.len += consume_size;

    /* part consume */
    rdb_object_calc_crc(rp, bb, consume_size);

    if (want_size == consume_size) {
        raw = nx_palloc(rp->o_pool, ob->len + 1);
        if ((ret = lzf_decompress(ob->tmp_val.data, ob->tmp_val.len, raw, ob->len)) == 0) {
            nx_pfree(rp->o_pool, ob->tmp_val.data);
            nx_pfree(rp->o_pool, raw);
            return OB_ERROR_LZF_DECOMPRESS;
        }

        raw[ob->len] = '\0';
        nx_str_set2(val, raw, ob->len);

        nx_pfree(rp->o_pool, ob->tmp_val.data);
        return OB_OVER;
    }
    return OB_ERROR_PREMATURE;
}

int
build_lzf_string_value(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, nx_str_t *val)
{
    int rc = 0;
    rdb_object_builder_t *sub_ob;
    int depth = ob->depth + 1;

    /*
    * 1. load compress length.
    * 2. load raw length.
    * 3. load lzf_string, and use lzf_decompress to decode.
    */
    
    /* sub builder */
    OB_LOOP_BEGIN(rp, sub_ob, depth)
    {
        /* sub process */
        switch (sub_ob->state) {
        case BUILD_LZF_STRING_IDLE:
        case BUILD_LZF_STRING_COMPRESS_LEN:
            rc = __build_lzf_string_compress_len(rp, sub_ob, bb);
            break;

        case BUILD_LZF_STRING_RAW_LEN:
            rc = __build_lzf_string_raw_len(rp, sub_ob, bb);
            break;

        case BUILD_LZF_STRING_LZF:
            rc = __build_lzf_string_lzf(rp, sub_ob, bb, val);
            break;

        default:
            rc = OB_ERROR_INVALID_NB_STATE;
            break;
        }

    }
    OB_LOOP_END(rp, rc)
    return rc;
}