#pragma once

#include "reply_parser_def.h"

/* redis-reply-builder error code */
#define RRB_OVER                        1
#define RRB_AGAIN                       0
#define RRB_ABORT                       -1
#define RRB_ERROR_PREMATURE             -2
#define RRB_ERROR_INVALID_PATH          -3
#define RRB_ERROR_INVALID_LEADING_CHAR  -4
#define RRB_ERROR_INVALID_RRB_STATE     -5
#define RRB_ERROR_INVALID_BULK_STRING   -6

#define RRB_LOOP_BEGIN(_rrp, _rrb, _depth)           \
    _rrb = stack_alloc_reply_builder(_rrp, _depth);  \
    do

#define RRB_LOOP_END(_rrp, _rc)                      \
    while (_rc == RRB_AGAIN);                        \
    if (_rc == RRB_OVER) {                           \
        stack_pop_reply_builder(_rrp);               \
    }

redis_reply_builder_t *  stack_push_reply_builder(redis_reply_parser_t *rrp);
void                     stack_pop_reply_builder(redis_reply_parser_t *rrp);
redis_reply_builder_t *  stack_alloc_reply_builder(redis_reply_parser_t *rrp, uint8_t depth);

redis_reply_builder_t *  stack_reply_builder_at(redis_reply_parser_t *rrp, uint8_t depth);
redis_reply_builder_t *  stack_reply_builder_top(redis_reply_parser_t *rrp);

/* EOF */