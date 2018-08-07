#pragma once

#include "reply_parser_def.h"

redis_reply_parser_t *  create_reply_parser(func_process_redis_reply cb, void *payload);
void                    destroy_reply_parser(redis_reply_parser_t *rrp);
void                    reset_reply_parser(redis_reply_parser_t *rrp);

int                     redis_reply_parse_once(redis_reply_parser_t *rrp, bip_buf_t *bb);

/* EOF */