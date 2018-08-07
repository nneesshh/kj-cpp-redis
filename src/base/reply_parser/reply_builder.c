#include "reply_builder.h"

redis_reply_builder_t *
stack_push_reply_builder(redis_reply_parser_t *rrp)
{
    redis_reply_builder_t *rb;

    rb = nx_array_push(rrp->stack_rrb);
    nx_memzero(rb, sizeof(redis_reply_builder_t));
    return rb;
}

void
stack_pop_reply_builder(redis_reply_parser_t *rrp)
{
    redis_reply_builder_t *rb;

    rb = nx_array_pop(rrp->stack_rrb);
    nx_memzero(rb, sizeof(redis_reply_builder_t));
}

redis_reply_builder_t *
stack_alloc_reply_builder(redis_reply_parser_t *rrp, uint8_t depth)
{
    redis_reply_builder_t *rb;

    rb = stack_reply_builder_at(rrp, depth);
    if (NULL == rb) {
        rb = stack_push_reply_builder(rrp);
        rb->depth = depth;
    }
    return rb;
}

redis_reply_builder_t *
stack_reply_builder_at(redis_reply_parser_t *rrp, uint8_t depth)
{
    return nx_array_at(rrp->stack_rrb, depth);
}

redis_reply_builder_t *
stack_reply_builder_top(redis_reply_parser_t *rrp)
{
    return nx_array_top(rrp->stack_rrb);
}