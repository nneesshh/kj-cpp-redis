#pragma once

#include "rdb_parser_def.h"

int  build_lzf_string_value(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, nx_str_t *val);

/* EOF */