#include "build_factory.h"

#include <assert.h>

int
build_footer(rdb_parser_t *rp, bip_buf_t *bb)
{
#ifdef CHECK_CRC
    size_t bytes;
    uint64_t checksum;

    rdb_object_t *o;

    o = rp->o;

    if (rp->version > CHECKSUM_VERSION_MIN) {
        bytes = 8;
        if (bip_buf_get_committed_size(bb) < bytes) {
            return OB_ERROR_PREMATURE;
        }

        checksum = (*(uint64_t *)bb->pos);
        o->checksum = checksum;
        assert(rp->chksum == o->checksum);
    }
#endif
    return OB_OVER;
}