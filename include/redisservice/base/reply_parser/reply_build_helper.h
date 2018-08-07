#pragma once

#include "reply_parser_def.h"

nx_chain_t *           alloc_simple_string_buf_chain_link(nx_pool_t *pool, nx_chain_t **ll);

void                   redis_reply_as_string(redis_reply_t *r, char *buf, size_t *len);

size_t                 redis_reply_read_leading(redis_reply_parser_t *rrp, bip_buf_t *bb, uint8_t *out);
size_t                 redis_reply_read_integer(redis_reply_parser_t *rrp, bip_buf_t *bb, int64_t *out);

/* EOF */