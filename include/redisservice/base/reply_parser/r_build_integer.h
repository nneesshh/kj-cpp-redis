#pragma once

#include "reply_parser_def.h"

int  r_build_integer(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, int64_t *intval);

/* EOF */