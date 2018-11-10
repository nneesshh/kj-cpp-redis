#include "reply_build_helper.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

nx_chain_t *
alloc_simple_string_buf_chain_link(nx_pool_t *pool, nx_chain_t **ll)
{
    nx_chain_t *cl, *ln;

    cl = nx_palloc(pool, sizeof(nx_chain_t));
    if (cl == NULL) {
        return NULL;
    }
    nx_memzero(cl, sizeof(nx_chain_t));

    cl->buf = NULL;
    cl->next = 0;

    /* link */
    if ((*ll) == NULL) {
        (*ll) = cl;
    }
    else {
        for (ln = (*ll); ln->next; ln = ln->next) { /* void */ }

        ln->next = cl;
    }
    return cl;
}

void
redis_reply_as_string(redis_reply_t *r, char *buf, size_t *len)
{
    nx_chain_t *ln;
    nx_buf_t *b;
    char *s;

    s = buf;

    for (ln = r->vall; ln; ln = ln->next) {
        b = ln->buf;
        nx_memcpy(s, b->pos, nx_buf_size(b));
        s += nx_buf_size(b);
    }

    (*len) = s - buf;
}

size_t
redis_reply_read_leading(redis_reply_parser_t *rrp, bip_buf_t *bb, uint8_t *out)
{
    size_t bytes = 1;
    uint8_t *ptr;

    if (bip_buf_get_committed_size(bb) < bytes)
        return 0;

    ptr = bip_buf_get_contiguous_block(bb);

    (*out) = (*ptr);
    return bytes;
}

size_t
redis_reply_read_integer(redis_reply_parser_t *rrp, bip_buf_t *bb, int64_t *out)
{
    size_t bytes = 2;
    char *p1, *p2, *ptr;
    size_t append_size, consume_size;
    int64_t v64, negative_mul;
    
    /* at least include "\r\n" */
    if (bip_buf_get_committed_size(bb) < bytes)
        return 0;

    p1 = bip_buf_get_contiguous_block(bb);
    p2 = bip_buf_find_str(bb, "\r\n", 2);

    if (NULL == p2)
        return 0;

    append_size = p2 - p1;
    consume_size = append_size + 2;

    /* to integer */
    v64 = 0;
    negative_mul = 1;
    for (ptr = p1; ptr < p2; ++ptr) {
        if (*ptr == '-') {
            negative_mul = -1;
            continue;
        }

        v64 = v64 * 10 + ((*ptr) - '0');
    }

    (*out) = v64 * negative_mul;
    return consume_size;
}