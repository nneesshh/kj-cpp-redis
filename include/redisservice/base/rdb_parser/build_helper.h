#pragma once

#include "rdb_parser_def.h"

#define CHECKSUM_VERSION_MIN  5

rdb_kv_chain_t *   alloc_rdb_kv_chain_link(nx_pool_t *pool, rdb_kv_chain_t **ll);
rdb_object_chain_t * alloc_rdb_object_chain_link(nx_pool_t *pool, rdb_object_chain_t **ll);

size_t  rdb_object_calc_crc(rdb_parser_t *rp, bip_buf_t *bb, size_t bytes);

size_t  rdb_object_read_store_len(rdb_parser_t *rp, bip_buf_t *bb, uint8_t *is_encoded, uint32_t *out);
size_t  rdb_object_read_int(rdb_parser_t *rp, bip_buf_t *bb, uint8_t enc, int32_t *out);
size_t  rdb_object_read_type(rdb_parser_t *rp, bip_buf_t *bb, uint8_t *out);

/* EOF */