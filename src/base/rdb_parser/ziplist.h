#pragma once

#include "rdb_parser.h"

void  load_ziplist_hash_or_zset(rdb_parser_t *rp, const char *zl, rdb_kv_chain_t **vall, size_t *size);
void  load_ziplist_list_or_set(rdb_parser_t *rp, const char *zl, rdb_kv_chain_t **vall, size_t *size);

void  ziplist_dump(rdb_parser_t *rp, const char *zl);

/* EOF */