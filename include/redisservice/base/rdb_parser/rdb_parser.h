#pragma once

#include "../redis_extern.h"
#include "rdb_parser_def.h"

MY_REDIS_EXTERN rdb_parser_t *  create_rdb_parser();
MY_REDIS_EXTERN void            destroy_rdb_parser(rdb_parser_t *rp);
MY_REDIS_EXTERN void            reset_rdb_parser(rdb_parser_t *rp);

MY_REDIS_EXTERN void            rdb_parse_bind_walk_cb(rdb_parser_t *rp, func_walk_rdb_object cb, void *payload);
MY_REDIS_EXTERN int             rdb_parse_object_once(rdb_parser_t *rp, bip_buf_t *bb);
MY_REDIS_EXTERN int             rdb_parse_dumped_data_once(rdb_parser_t *rp, bip_buf_t *bb);
MY_REDIS_EXTERN int             rdb_parse_dumped_data(rdb_parser_t *rp, func_walk_rdb_object cb, void *payload, const char *s, size_t len);

MY_REDIS_EXTERN int             rdb_parse_file(rdb_parser_t *rp, const char *path);

/* EOF */