#include "r_build_factory.h"

int
r_build_integer(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, int64_t *intval)
{
    size_t n;

    (void *)rrb;

    if ((n = redis_reply_read_integer(rrp, bb, intval)) == 0)
        return RRB_ERROR_PREMATURE;

    bip_buf_decommit(bb, n);
    rrp->parsed += n;
    return RRB_OVER;
}