#include "rdb_parser.h"
#include <time.h>
#include <assert.h>

#ifdef _MSC_VER
# ifdef _DEBUG
#  include <vld.h>
# endif
#endif

#include "rdb_object_builder.h"
#include "build_factory.h"

#define MAGIC_VERSION                 5

#define RDB_PARSER_INPUT_BIPBUF_SIZE  4096
#define RDB_PARSER_POOL_SIZE          1024
#define RDB_PARSER_OBJECT_POOL_SIZE   4096

enum PARSE_RDB {
    PARSE_RDB_IDLE = 0,
    PARSE_RDB_HEADER,
    PARSE_RDB_BODY,
    PARSE_RDB_FOOTER,
    PARSE_RDB_OVER,
};

int 
__default_walk_rdb_object(rdb_object_t *o, void *payload)
{
    (void *)payload;
    fprintf(stderr, "\n[__default_walk_rdb_object()] not bind any walk cb yet!!!\n");
    return -1;
}

rdb_parser_t *
create_rdb_parser(func_walk_rdb_object cb, void *payload)
{
    rdb_parser_t *rp = nx_alloc(sizeof(rdb_parser_t));
    nx_memzero(rp, sizeof(rdb_parser_t));

    /* NOTE: trick here, version set 5 as we want to calculate crc where read version field. */
    rp->version = MAGIC_VERSION;
    rp->chksum = 0;
    rp->parsed = 0;
    rp->state = PARSE_RDB_IDLE;

    rp->in_bb = bip_buf_create(RDB_PARSER_INPUT_BIPBUF_SIZE);
    rp->pool = nx_create_pool(RDB_PARSER_POOL_SIZE);

    rp->stack_ob = nx_array_create(rp->pool, 16, sizeof(rdb_object_builder_t));

    rp->o = nx_palloc(rp->pool, sizeof(rdb_object_t));
    rdb_object_init(rp->o);

    rp->o_pool = nx_create_pool(RDB_PARSER_OBJECT_POOL_SIZE);
    rp->o_cb = __default_walk_rdb_object;
    rp->o_payload = NULL;

    return rp;
}

void
destroy_rdb_parser(rdb_parser_t *rp)
{
    bip_buf_destroy(rp->in_bb);
    nx_destroy_pool(rp->o_pool);
    nx_destroy_pool(rp->pool);
    nx_free(rp);
}

void
reset_rdb_parser(rdb_parser_t *rp)
{
    nx_pool_t  *p, *tmp;

    rp->chksum = 0;
    rp->parsed = 0;
    rp->state = PARSE_RDB_IDLE;

    /* reset and shrink object pool*/
    nx_reset_pool(rp->o_pool);
    p = rp->o_pool->d.next;
    while (p) {
        tmp = p->d.next;
        nx_free(p);
        p = tmp;
    }
    rp->o_pool->d.next = NULL;

    /* reset and shrink parser pool*/
    nx_reset_pool(rp->pool);
    p = rp->pool->d.next;
    while (p) {
        tmp = p->d.next;
        nx_free(p);
        p = tmp;
    }
    rp->pool->d.next = NULL;

    bip_buf_reset(rp->in_bb);

    rp->stack_ob = nx_array_create(rp->pool, 16, sizeof(rdb_object_builder_t));

    rp->o = nx_palloc(rp->pool, sizeof(rdb_object_t));
    rdb_object_init(rp->o);
}

void
rdb_parse_bind_walk_cb(rdb_parser_t *rp, func_walk_rdb_object cb, void *payload)
{
    rp->o_cb = cb;
    rp->o_payload = payload;
}

int
rdb_parse_object_once(rdb_parser_t *rp, bip_buf_t *bb)
{
    int rc;

    rc = OB_AGAIN;

    while (rp->state != PARSE_RDB_OVER
        && (rc == OB_AGAIN || rc == OB_OVER)) {

        switch (rp->state) {
        case PARSE_RDB_IDLE:
        case PARSE_RDB_HEADER:
            /* header */
            if ((rc = build_header(rp, bb)) != OB_OVER) {
                break;
            }

            rp->state = PARSE_RDB_BODY;
            break;

        case PARSE_RDB_BODY:
            /* body */
            if ((rc = build_body(rp, bb)) != OB_OVER) {
                break;
            }

            rp->state = PARSE_RDB_FOOTER;
            break;

        case PARSE_RDB_FOOTER:
            /* footer */
            if ((rc = build_footer(rp, bb)) != OB_OVER) {
                break;
            }

            rp->state = PARSE_RDB_OVER;
            break;

        default:
            break;
        }
    }
    return rc;
}

int
rdb_parse_dumped_data_once(rdb_parser_t *rp, bip_buf_t *bb)
{
    int rc;

    rc = OB_AGAIN;

    while (rc != OB_ERROR_PREMATURE
        && rp->state != PARSE_RDB_OVER) {

        rc = build_dumped_data(rp, bb);
    }
    return rc;
}

int
rdb_parse_dumped_data(rdb_parser_t *rp, func_walk_rdb_object cb, void *payload, const char *s, size_t len)
{
    int rc;

    bip_buf_t *bb;
    char *p1;
    const char *s1;
    size_t reserved, consume, remain;
    
    reset_rdb_parser(rp);
    rdb_parse_bind_walk_cb(rp, cb, payload);

    bb = rp->in_bb;
    bip_buf_reset(bb);
    
    /* Write the footer, this is how it looks like:
    * ----------------+---------------------+---------------+
    * ... RDB payload | 2 bytes RDB version | 8 bytes CRC64 |
    * ----------------+---------------------+---------------+
    * RDB version and CRC are both in little endian.
    */
    /* 10 = 2 version + 8 crc */
    remain = len - 10;
    s1 = s;

    while (remain > 0) {
        p1 = bip_buf_reserve(bb, &reserved);

#define __max(a,b) (((a) > (b)) ? (a) : (b))
#define __min(a,b) (((a) < (b)) ? (a) : (b))

        consume = __min(reserved, remain);
        remain -= consume;
        nx_memcpy(p1, s1, consume);
        bip_buf_commit(bb, consume);
        s1 += consume;

        rc = rdb_parse_dumped_data_once(rp, bb);
    }

    return rc;
}

int
rdb_parse_file(rdb_parser_t *rp, const char *path)
{
    int rc, is_eof;

    FILE *r_fp;
    size_t bytes, reserved;
    bip_buf_t *bb;
    uint8_t *ptr, *ptr_r;

    r_fp = fopen(path, "rb");
    bb = rp->in_bb;

    ptr = bip_buf_get_contiguous_block(bb);

    rc = OB_AGAIN;
    is_eof = 0;

    while (!is_eof
        && PARSE_RDB_OVER != rp->state) {

        /* init reserved buffer */
        reserved = 0;
        ptr_r = bip_buf_reserve(bb, &reserved);
        if (NULL == ptr_r) {
            rc = OB_ERROR_PREMATURE;
            break;
        }

        /* read file into reserved buffer */
        reserved = bip_buf_get_reservation_size(bb);
        bytes = fread(ptr_r, 1, reserved, r_fp);
        if (0 == bytes || bytes < reserved) {
            is_eof = 1;

            if (0 == bytes) {
                rc = OB_ERROR_PREMATURE;
                break;
            }
        }
        bip_buf_commit(bb, bytes);

        /* consume */
        rc = rdb_parse_object_once(rp, bb);
    }

    fclose(r_fp);
    return rc;
}
