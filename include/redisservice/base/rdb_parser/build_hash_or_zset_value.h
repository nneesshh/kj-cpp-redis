#pragma once

#include "rdb_parser_def.h"

int  build_hash_or_zset_value(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, rdb_kv_chain_t **vall, size_t *size);

/* EOF */