#pragma once

#include "rdb_parser_def.h"

void load_zipmap(rdb_parser_t *rp, const char *zm, rdb_kv_chain_t **vall, size_t *size);

void zipmap_dump(rdb_parser_t *rp, const char *s);

/* EOF */