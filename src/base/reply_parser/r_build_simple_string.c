#include "r_build_factory.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define __max(a,b) (((a) > (b)) ? (a) : (b))
#define __min(a,b) (((a) < (b)) ? (a) : (b))

#define SIMPLE_STRING_TEMP_BUFF_SIZE  4096

enum BUILD_SIMPLE_STRING_TAG {
    BUILD_SIMPLE_STRING_IDLE = 0,
    BUILD_SIMPLE_STRING_INIT,
    BUILD_SIMPLE_STRING_PLAIN,
};

static int __build_simple_string_init(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);
static int __build_simple_string_plain(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);

static int
__build_simple_string_init(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    (void *)bb;

    r->vall_tail = alloc_simple_string_buf_chain_link(rrp->r_pool, &r->vall);
    r->vall_tail->buf = nx_create_temp_buf(rrp->r_pool, SIMPLE_STRING_TEMP_BUFF_SIZE);

    rrb->store_len = 0; /* string len */
    rrb->len = 1; /* array len */

    /* next state */
    rrb->state = BUILD_SIMPLE_STRING_PLAIN;
    return RRB_AGAIN;
}

static int
__build_simple_string_plain(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    size_t buf_size, consume_size, remain_size, append_size, freespace;
    char *p1, *p2, *s;
    int end_with_r, reply_ok;

    nx_buf_t *b;
    nx_chain_t *ln;

    buf_size = bip_buf_get_committed_size(bb);
    /* at least include "\r\n" */
    if (buf_size < 2)
        return RRB_ERROR_PREMATURE;

    p1 = bip_buf_get_contiguous_block(bb);
    p2 = bip_buf_find_str(bb, "\r\n", 2);
    remain_size = consume_size = 0;

    reply_ok = 0;

    /* consume */
    if (NULL == p2) {
        end_with_r = ('\r' == *(char *)(p1 + buf_size - 1));
        /* leave one char if it is '\r' */
        consume_size = end_with_r ? buf_size - 1 : buf_size;
        remain_size = consume_size;
    }
    else {
        consume_size = p2 + 2 - p1;
        remain_size = consume_size - 2;

        reply_ok = 1;
    }

    while (remain_size > 0) {
        /* tail buf */
        b = r->vall_tail->buf;

        /* check free space */
        freespace = b->end - b->last;
        append_size = __min(freespace, remain_size);
        if (0 == append_size) {
            r->vall_tail = alloc_simple_string_buf_chain_link(rrp->r_pool, &r->vall_tail);
            r->vall_tail->buf = nx_create_temp_buf(rrp->r_pool, SIMPLE_STRING_TEMP_BUFF_SIZE);

            ++rrb->len;
            continue;
        }

        /* append */
        s = b->pos;
        nx_memcpy(s, p1, append_size);
        rrb->store_len += append_size;

        /* remain */
        b->last += append_size;
        remain_size -= append_size;
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
r_build_simple_string(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    int rc = 0;
    redis_reply_builder_t *sub_rrb;
    int depth = rrb->depth + 1;

    /* sub builder */
    RRB_LOOP_BEGIN(rrp, sub_rrb, depth)
    {
        /* sub process */
        switch (sub_rrb->state) {
        case BUILD_SIMPLE_STRING_IDLE:
        case BUILD_SIMPLE_STRING_INIT:
            rc = __build_simple_string_init(rrp, sub_rrb, bb, r);
            break;

        case BUILD_SIMPLE_STRING_PLAIN:
            rc = __build_simple_string_plain(rrp, sub_rrb, bb, r);
            break;

        default:
            rc = RRB_ERROR_INVALID_RRB_STATE;
            break;
        }

    }
    RRB_LOOP_END(rrp, rc)
    return rc;
}
