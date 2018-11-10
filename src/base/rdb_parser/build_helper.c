#include "build_helper.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "lzf.h"
#include "../crc64.h"
#include "../endian.h"

#define REDIS_RDB_6B    0
#define REDIS_RDB_14B   1
#define REDIS_RDB_32B   2
#define REDIS_RDB_ENCV  3

rdb_kv_chain_t *
alloc_rdb_kv_chain_link(nx_pool_t *pool, rdb_kv_chain_t **ll)
{
    rdb_kv_chain_t *kvcl, *ln;

    kvcl = nx_palloc(pool, sizeof(rdb_kv_chain_t));
    if (kvcl == NULL) {
        return NULL;
    }
    nx_memzero(kvcl, sizeof(rdb_kv_chain_t));

    kvcl->kv = nx_palloc(pool, sizeof(rdb_kv_t));
    if (kvcl->kv == NULL) {
        nx_pfree(pool, kvcl);
        return NULL;
    }
    nx_memzero(kvcl->kv, sizeof(rdb_kv_t));

	/* link */
	if ((*ll) == NULL) {
		(*ll) = kvcl;
	}
	else {
		for (ln = (*ll); ln->next; ln = ln->next) { /* void */ }

		ln->next = kvcl;
	}
    return kvcl;
}

rdb_object_chain_t *
alloc_rdb_object_chain_link(nx_pool_t *pool, rdb_object_chain_t **ll)
{
    rdb_object_chain_t *cl, *ln;

    cl = nx_palloc(pool, sizeof(rdb_object_chain_t));
    if (cl == NULL) {
        return NULL;
    }
    nx_memzero(cl, sizeof(rdb_object_chain_t));

    cl->elem = nx_palloc(pool, sizeof(rdb_object_t));
    if (cl->elem == NULL) {
        nx_pfree(pool, cl);
        return NULL;
    }
    nx_memzero(cl->elem, sizeof(rdb_object_t));

    cl->elem->expire = -1;
    cl->next = NULL;

	/* link */
	if ((*ll) == NULL) {
		(*ll) = cl;
	}
	else {
		for (ln = (*ll); ln->next; ln = ln->next) { /* void */ }

		ln->next = cl;
	}
    return cl;
}

size_t
rdb_object_calc_crc(rdb_parser_t *rp, bip_buf_t *bb, size_t bytes)
{
    assert(bip_buf_get_committed_size(bb) >= bytes);

#ifdef CHECK_CRC
    if (rp->version >= CHECKSUM_VERSION_MIN) {
        rp->chksum = crc64(rp->chksum, bb->pos, bytes);
    }
#endif

	bip_buf_decommit(bb, bytes);
    rp->parsed += bytes;
    return bytes;
}

size_t
rdb_object_read_store_len(rdb_parser_t *rp, bip_buf_t *bb, uint8_t *is_encoded, uint32_t *out)
{
    size_t bytes = 1;
    uint8_t *p;
    uint8_t type;
    uint32_t v32;

    if (bip_buf_get_committed_size(bb) < bytes) {
        return 0;
    }

    p = bip_buf_get_contiguous_block(bb);

    if (is_encoded)
        *is_encoded = 0;

    type = ((*p) & 0xc0) >> 6;

    /**
    * 00xxxxxx, then the next 6 bits represent the length
    * 01xxxxxx, then the next 14 bits represent the length
    * 10xxxxxx, then the next 6 bits is discarded, and next 4bytes represent the length(BigEndian)
    * 11xxxxxx, The remaining 6 bits indicate the format
    */
    if (REDIS_RDB_6B == type) {
        (*out) = (*p) & 0x3f;
    }
    else if (REDIS_RDB_14B == type) {

        bytes = 2;

        if (bip_buf_get_committed_size(bb) < bytes) {
            return 0;
        }

        (*out) = (((*p) & 0x3f) << 8) | (*(p + 1));
    }
    else if (REDIS_RDB_32B == type) {

        bytes = 5;

        if (bip_buf_get_committed_size(bb) < bytes) {
            return 0;
        }

        memcpy(&v32, p + 1, 4);
        memrev32(&v32);
        (*out) = v32;
    }
    else {
        if (is_encoded)
            *is_encoded = 1;

        (*out) = (*p) & 0x3f;
    }
    return bytes;
}

size_t
rdb_object_read_int(rdb_parser_t *rp, bip_buf_t *bb, uint8_t enc, int32_t *out)
{
    size_t bytes = 1;
    uint8_t *p;

    if (bip_buf_get_committed_size(bb) < bytes) {
        return 0;
    }

    p = bip_buf_get_contiguous_block(bb);

    if (RDB_ENC_INT8 == enc) {
        (*out) = (*p);
    }
    else if (RDB_ENC_INT16 == enc) {

        bytes = 2;

        if (bip_buf_get_committed_size(bb) < bytes) {
            return 0;
        }

        (*out) = (int16_t)((*p) | (*(p + 1)) << 8);
    }
    else {
        bytes = 4;

        if (bip_buf_get_committed_size(bb) < bytes) {
            return 0;
        }

        (*out) = (int32_t)((*p) | ((*(p + 1)) << 8) | ((*(p + 2)) << 16) | ((*(p + 3)) << 24));
    }
    return bytes;
}


size_t
rdb_object_read_type(rdb_parser_t *rp, bip_buf_t *bb, uint8_t *out)
{
	size_t bytes = 1;
	uint8_t *ptr;

	if (bip_buf_get_committed_size(bb) < bytes) {
		return 0;
	}

	ptr = bip_buf_get_contiguous_block(bb);

	(*out) = (*ptr);
	return bytes;
}
