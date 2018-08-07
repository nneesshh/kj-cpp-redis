#pragma once

#include "rdb_parser_def.h"

/* object-builder error code */
#define OB_OVER                        1
#define OB_AGAIN                       0
#define OB_ABORT                       -1
#define OB_ERROR_PREMATURE             -2
#define OB_ERROR_INVALID_PATH          -3
#define OB_ERROR_INVALID_MAGIC_STRING  -4
#define OB_ERROR_INVALID_NB_TYPE       -5
#define OB_ERROR_INVALID_STING_ENC     -6
#define OB_ERROR_INVALID_NB_STATE      -7
#define OB_ERROR_LZF_DECOMPRESS        -8

#define OB_LOOP_BEGIN(_rp, _nb, _depth)             \
    _nb = stack_alloc_object_builder(_rp, _depth);  \
    do

#define OB_LOOP_END(_rp, _rc)                       \
    while (_rc == OB_AGAIN);                        \
    if (_rc == OB_OVER) {                           \
        stack_pop_object_builder(_rp);              \
    }

rdb_object_builder_t *  stack_push_object_builder(rdb_parser_t *rp);
void                    stack_pop_object_builder(rdb_parser_t *rp);
rdb_object_builder_t *  stack_alloc_object_builder(rdb_parser_t *rp, uint8_t depth);

rdb_object_builder_t *  stack_object_builder_at(rdb_parser_t *rp, uint8_t depth);
rdb_object_builder_t *  stack_object_builder_top(rdb_parser_t *rp);

/* EOF */