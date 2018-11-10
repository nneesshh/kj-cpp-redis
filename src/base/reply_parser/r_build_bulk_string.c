#include "r_build_factory.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define __max(a,b) (((a) > (b)) ? (a) : (b))
#define __min(a,b) (((a) < (b)) ? (a) : (b))

#define BULK_STRING_TEMP_BUFF_SIZE  4096

enum BUILD_BULK_STRING_TAG {
    BUILD_BULK_STRING_IDLE = 0,
    BUILD_BULK_STRING_STORE_LEN,
    BUILD_BULK_STRING_PLAIN,
};

static int __build_bulk_string_store_len(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);
static int __build_bulk_string_plain(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);

static int
__build_bulk_string_store_len(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    int rc;
    int64_t len;

    (void *)r;

    rc = r_build_integer(rrp, rrb, bb, &len);

    if (rc == RRB_OVER) {
        if (len >= 0) {
            /* "len==0" means empty, format: "$0\r\n\r\n" */
            rrb->store_len = (uint32_t)len; /* string len */
            rrb->len = 1; /* array len */

            /* pre-alloc */
            r->vall_tail = alloc_simple_string_buf_chain_link(rrp->r_pool, &r->vall);
            r->vall_tail->buf = nx_create_temp_buf(rrp->r_pool, rrb->store_len + 3); /* 3 means with tail "\r\n" and "\0" */

            /* next state */
            rrb->state = BUILD_BULK_STRING_PLAIN;
            return RRB_AGAIN;
        }
        else {
            /* null, format: "$-1\r\n" */
            r->is_null = 1;
            r->vall_tail = NULL;
            r->bytes = 0;
        }
    }

    return rc;
}

static int
__build_bulk_string_plain(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    size_t buf_size, want_size, consume_size, append_size;
    char *p1, *p2, *s;

    nx_buf_t *b;
    int reply_ok;

    buf_size = bip_buf_get_committed_size(bb);
    p1 = bip_buf_get_contiguous_block(bb);

    /* tail buf */
    b = r->vall_tail->buf;
    reply_ok = 0;

    want_size = rrb->store_len + 2 - nx_buf_size(b);
    consume_size = (buf_size < want_size) ? buf_size : want_size;
    append_size = __min(want_size, consume_size);

    /* append */
    s = b->last;
    nx_memcpy(s, p1, append_size);
    b->last += append_size;

    if (consume_size == want_size) {
        assert(rrb->store_len + 2 == nx_buf_size(b));

        /* validate end sequence */
        p2 = b->last - 2;
        if (p2[0] != '\r' || p2[1] != '\n') {
            return RRB_ERROR_INVALID_BULK_STRING;
        }
                
        /* trim tail"\r\n" */
        b->last -= 2;
        
        reply_ok = 1;
    }

    /* part consume */
    bip_buf_decommit(bb, consume_size);
    rrp->parsed += consume_size;

    if (reply_ok) {
        r->bytes = rrb->store_len;
        return RRB_OVER;
    }

    return RRB_ERROR_PREMATURE;
}

int
r_build_bulk_string(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    int rc = 0;
    redis_reply_builder_t *sub_rrb;
    int depth = rrb->depth + 1;

    /* sub builder */
    RRB_LOOP_BEGIN(rrp, sub_rrb, depth)
    {
        /* sub process */
        switch (sub_rrb->state) {
        case BUILD_BULK_STRING_IDLE:
        case BUILD_BULK_STRING_STORE_LEN:
            rc = __build_bulk_string_store_len(rrp, sub_rrb, bb, r);
            break;

        case BUILD_BULK_STRING_PLAIN:
            rc = __build_bulk_string_plain(rrp, sub_rrb, bb, r);
            break;

        default:
            rc = RRB_ERROR_INVALID_RRB_STATE;
            break;
        }

    }
    RRB_LOOP_END(rrp, rc)
    return rc;
}
