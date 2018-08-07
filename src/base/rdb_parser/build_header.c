#include "build_factory.h"

#include <stdlib.h>
#include <memory.h>

#define MAGIC_STR  "REDIS"

int
build_header(rdb_parser_t *rp, bip_buf_t *bb)
{
    char chversion[5];
    size_t bytes;
	uint8_t *ptr;

    /* magic string(5bytes) and version(4bytes) */
    bytes = 9;

    if (bip_buf_get_committed_size(bb) < bytes) {
        return OB_ERROR_PREMATURE;
    }

	ptr = bip_buf_get_contiguous_block(bb);
    if (memcmp(ptr, MAGIC_STR, 5) != 0)
        return OB_ERROR_INVALID_MAGIC_STRING;

    nx_memcpy(chversion, ptr + 5, 4);
    chversion[4] = '\0';
    rp->version = atoi(chversion);

    /* ok */
    rdb_object_calc_crc(rp, bb, bytes);
    return OB_OVER;
}