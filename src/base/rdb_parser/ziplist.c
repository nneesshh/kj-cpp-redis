/* The ziplist is a specially encoded dually linked list that is designed
* to be very memory efficient. It stores both strings and integer values,
* where integers are encoded as actual integers instead of a series of
* characters. It allows push and pop operations on either side of the list
* in O(1) time. However, because every operation requires a reallocation of
* the memory used by the ziplist, the actual complexity is related to the
* amount of memory used by the ziplist.
*
* ----------------------------------------------------------------------------
*
* ZIPLIST OVERALL LAYOUT
* ======================
*
* The general layout of the ziplist is as follows:
*
* <zlbytes> <zltail> <zllen> <entry> <entry> ... <entry> <zlend>
*
* NOTE: all fields are stored in little endian, if not specified otherwise.
*
* <uint32_t zlbytes> is an unsigned integer to hold the number of bytes that
* the ziplist occupies, including the four bytes of the zlbytes field itself.
* This value needs to be stored to be able to resize the entire structure
* without the need to traverse it first.
*
* <uint32_t zltail> is the offset to the last entry in the list. This allows
* a pop operation on the far side of the list without the need for full
* traversal.
*
* <uint16_t zllen> is the number of entries. When there are more than
* 2^16-2 entries, this value is set to 2^16-1 and we need to traverse the
* entire list to know how many items it holds.
*
* <uint8_t zlend> is a special entry representing the end of the ziplist.
* Is encoded as a single byte equal to 255. No other normal entry starts
* with a byte set to the value of 255.
*
* ZIPLIST ENTRIES
* ===============
*
* Every entry in the ziplist is prefixed by metadata that contains two pieces
* of information. First, the length of the previous entry is stored to be
* able to traverse the list from back to front. Second, the entry encoding is
* provided. It represents the entry type, integer or string, and in the case
* of strings it also represents the length of the string payload.
* So a complete entry is stored like this:
*
* <prevlen> <encoding> <entry-data>
*
* Sometimes the encoding represents the entry itself, like for small integers
* as we'll see later. In such a case the <entry-data> part is missing, and we
* could have just:
*
* <prevlen> <encoding>
*
* The length of the previous entry, <prevlen>, is encoded in the following way:
* If this length is smaller than 254 bytes, it will only consume a single
* byte representing the length as an unsinged 8 bit integer. When the length
* is greater than or equal to 254, it will consume 5 bytes. The first byte is
* set to 254 (FE) to indicate a larger value is following. The remaining 4
* bytes take the length of the previous entry as value.
*
* So practically an entry is encoded in the following way:
*
* <prevlen from 0 to 253> <encoding> <entry>
*
* Or alternatively if the previous entry length is greater than 253 bytes
* the following encoding is used:
*
* 0xFE <4 bytes unsigned little endian prevlen> <encoding> <entry>
*
* The encoding field of the entry depends on the content of the
* entry. When the entry is a string, the first 2 bits of the encoding first
* byte will hold the type of encoding used to store the length of the string,
* followed by the actual length of the string. When the entry is an integer
* the first 2 bits are both set to 1. The following 2 bits are used to specify
* what kind of integer will be stored after this header. An overview of the
* different types and encodings is as follows. The first byte is always enough
* to determine the kind of entry.
*
* |00pppppp| - 1 byte
*      String value with length less than or equal to 63 bytes (6 bits).
*      "pppppp" represents the unsigned 6 bit length.
* |01pppppp|qqqqqqqq| - 2 bytes
*      String value with length less than or equal to 16383 bytes (14 bits).
*      IMPORTANT: The 14 bit number is stored in big endian.
* |10000000|qqqqqqqq|rrrrrrrr|ssssssss|tttttttt| - 5 bytes
*      String value with length greater than or equal to 16384 bytes.
*      Only the 4 bytes following the first byte represents the length
*      up to 32^2-1. The 6 lower bits of the first byte are not used and
*      are set to zero.
*      IMPORTANT: The 32 bit number is stored in big endian.
* |11000000| - 3 bytes
*      Integer encoded as int16_t (2 bytes).
* |11010000| - 5 bytes
*      Integer encoded as int32_t (4 bytes).
* |11100000| - 9 bytes
*      Integer encoded as int64_t (8 bytes).
* |11110000| - 4 bytes
*      Integer encoded as 24 bit signed (3 bytes).
* |11111110| - 2 bytes
*      Integer encoded as 8 bit signed (1 byte).
* |1111xxxx| - (with xxxx between 0000 and 1101) immediate 4 bit integer.
*      Unsigned integer from 0 to 12. The encoded value is actually from
*      1 to 13 because 0000 and 1111 can not be used, so 1 should be
*      subtracted from the encoded 4 bit value to obtain the right value.
* |11111111| - End of ziplist special entry.
*
* Like for the ziplist header, all the integers are represented in little
* endian byte order, even when this code is compiled in big endian systems.
*
* EXAMPLES OF ACTUAL ZIPLISTS
* ===========================
*
* The following is a ziplist containing the two elements representing
* the strings "2" and "5". It is composed of 15 bytes, that we visually
* split into sections:
*
*  [0f 00 00 00] [0c 00 00 00] [02 00] [00 f3] [02 f6] [ff]
*        |             |          |       |       |     |
*     zlbytes        zltail    entries   "2"     "5"   end
*
* The first 4 bytes represent the number 15, that is the number of bytes
* the whole ziplist is composed of. The second 4 bytes are the offset
* at which the last ziplist entry is found, that is 12, in fact the
* last entry, that is "5", is at offset 12 inside the ziplist.
* The next 16 bit integer represents the number of elements inside the
* ziplist, its value is 2 since there are just two elements inside.
* Finally "00 f3" is the first entry representing the number 2. It is
* composed of the previous entry length, which is zero because this is
* our first entry, and the byte F3 which corresponds to the encoding
* |1111xxxx| with xxxx between 0001 and 1101. We need to remove the "F"
* higher order bits 1111, and subtract 1 from the "3", so the entry value
* is "2". The next entry has a prevlen of 02, since the first entry is
* composed of exactly two bytes. The entry itself, F6, is encoded exactly
* like the first entry, and 6-1 = 5, so the value of the entry is 5.
* Finally the special entry FF signals the end of the ziplist.
*
* Adding another element to the above string with the value "Hello World"
* allows us to show how the ziplist encodes small strings. We'll just show
* the hex dump of the entry itself. Imagine the bytes as following the
* entry that stores "5" in the ziplist above:
*
* [02] [0b] [48 65 6c 6c 6f 20 57 6f 72 6c 64]
*
* The first byte, 02, is the length of the previous entry. The next
* byte represents the encoding in the pattern |00pppppp| that means
* that the entry is a string of length <pppppp>, so 0B means that
* an 11 bytes string follows. From the third byte (48) to the last (64)
* there are just the ASCII characters for "Hello World".
*
* ----------------------------------------------------------------------------
*
* Copyright (c) 2009-2012, Pieter Noordhuis <pcnoordhuis at gmail dot com>
* Copyright (c) 2009-2017, Salvatore Sanfilippo <antirez at gmail dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   * Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*   * Neither the name of Redis nor the names of its contributors may be used
*     to endorse or promote products derived from this software without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ziplist.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../mysnprintf.h"
#include "../endian.h"
#include "build_helper.h"

#define ZIP_END 255         /* Special "end of ziplist" entry. */
#define ZIP_BIG_PREVLEN 254 /* Max number of bytes of the previous entry, for
                               the "prevlen" field prefixing each entry, to be
                               represented with just a single byte. Otherwise
                               it is represented as FF AA BB CC DD, where
                               AA BB CC DD are a 4 bytes unsigned integer
                               representing the previous entry len. */

/* Different encoding/length possibilities */
#define ZIP_STR_MASK 0xc0
#define ZIP_INT_MASK 0x30
#define ZIP_STR_06B (0 << 6)
#define ZIP_STR_14B (1 << 6)
#define ZIP_STR_32B (2 << 6)
#define ZIP_INT_16B (0xc0 | 0<<4)
#define ZIP_INT_32B (0xc0 | 1<<4)
#define ZIP_INT_64B (0xc0 | 2<<4)
#define ZIP_INT_24B (0xc0 | 3<<4)
#define ZIP_INT_8B 0xfe

/* 4 bit integer immediate encoding |1111xxxx| with xxxx between
 * 0001 and 1101. */
#define ZIP_INT_IMM_MASK 0x0f   /* Mask to extract the 4 bits value. To add
                                   one is needed to reconstruct the value. */
#define ZIP_INT_IMM_MIN 0xf1    /* 11110001 */
#define ZIP_INT_IMM_MAX 0xfd    /* 11111101 */

#define INT24_MAX 0x7fffff
#define INT24_MIN (-INT24_MAX - 1)

/* Macro to determine if the entry is a string. String entries never start
 * with "11" as most significant bits of the first byte. */
#define ZIP_IS_STR(enc) (((enc) & ZIP_STR_MASK) < ZIP_STR_MASK)

/* Utility macros.*/

/* Return total bytes a ziplist is composed of. */
#define ZIPLIST_BYTES(zl)       (*((uint32_t*)(zl)))

/* Return the offset of the last item inside the ziplist. */
#define ZIPLIST_TAIL_OFFSET(zl) (*((uint32_t*)((zl)+sizeof(uint32_t))))

/* Return the length of a ziplist, or UINT16_MAX if the length cannot be
 * determined without scanning the whole ziplist. */
#define ZIPLIST_LENGTH(zl)      (*((uint16_t*)((zl)+sizeof(uint32_t)*2)))

/* The size of a ziplist header: two 32 bit integers for the total
 * bytes count and last item offset. One 16 bit integer for the number
 * of items field. */
#define ZIPLIST_HEADER_SIZE     (sizeof(uint32_t)*2+sizeof(uint16_t))

/* Size of the "end of ziplist" entry. Just one byte. */
#define ZIPLIST_END_SIZE        (sizeof(uint8_t))

/* Return the pointer to the first entry of a ziplist. */
#define ZIPLIST_ENTRY_HEAD(zl)  ((zl)+ZIPLIST_HEADER_SIZE)

/* Return the pointer to the last entry of a ziplist, using the
 * last entry offset inside the ziplist header. */
/* #define ZIPLIST_ENTRY_TAIL(zl)  ((zl)+intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))) */

/* Return the pointer to the last byte of a ziplist, which is, the
 * end of ziplist FF entry. */
/* #define ZIPLIST_ENTRY_END(zl)   ((zl)+intrev32ifbe(ZIPLIST_BYTES(zl))-1) */

/* Increment the number of items field in the ziplist header. Note that this
 * macro should never overflow the unsigned 16 bit integer, since entries are
 * always pushed one at a time. When UINT16_MAX is reached we want the count
 * to stay there to signal that a full scan is needed to get the number of
 * items inside the ziplist. */
/* #define ZIPLIST_INCR_LENGTH(zl,incr) { \
    if (ZIPLIST_LENGTH(zl) < UINT16_MAX) \
        ZIPLIST_LENGTH(zl) = intrev16ifbe(intrev16ifbe(ZIPLIST_LENGTH(zl))+incr); \
} */

/* We use this function to receive information about a ziplist entry.
 * Note that this is not how the data is actually encoded, is just what we
 * get filled by a function in order to operate more easily. */
typedef struct zlentry {
    unsigned int prevrawlensize; /* Bytes used to encode the previous entry len*/
    unsigned int prevrawlen;     /* Previous entry len. */
    unsigned int lensize;        /* Bytes used to encode this entry type/len.
                                    For example strings have a 1, 2 or 5 bytes
                                    header. Integers always use a single byte.*/
    unsigned int len;            /* Bytes used to represent the actual entry.
                                    For strings this is just the string length
                                    while for integers it is 1, 2, 3, 4, 8 or
                                    0 (for 4 bit immediate) depending on the
                                    number range. */
    unsigned int headersize;     /* prevrawlensize + lensize. */
    unsigned char encoding;      /* Set to ZIP_STR_* or ZIP_INT_* depending on
                                    the entry encoding. However for 4 bits
                                    immediate integers this can assume a range
                                    of values and must be range-checked. */
    unsigned char *p;            /* Pointer to the very start of the entry, that
                                    is, this points to prev-entry-len field. */
} zlentry;

#define ZIPLIST_ENTRY_ZERO(zle) { \
    (zle)->prevrawlensize = (zle)->prevrawlen = 0; \
    (zle)->lensize = (zle)->len = (zle)->headersize = 0; \
    (zle)->encoding = 0; \
    (zle)->p = NULL; \
}

/* Extract the encoding from the byte pointed by 'ptr' and set it into
 * 'encoding' field of the zlentry structure. */
#define ZIP_ENTRY_ENCODING(ptr, encoding) do {  \
    (encoding) = (ptr[0]); \
    if ((encoding) < ZIP_STR_MASK) (encoding) &= ZIP_STR_MASK; \
} while(0)

/* Return bytes needed to store integer encoded by 'encoding'. */
unsigned int zipIntSize(unsigned char encoding) {
    switch(encoding) {
    case ZIP_INT_8B:  return 1;
    case ZIP_INT_16B: return 2;
    case ZIP_INT_24B: return 3;
    case ZIP_INT_32B: return 4;
    case ZIP_INT_64B: return 8;
    }
    if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX)
        return 0; /* 4 bit immediate */
    fprintf(stderr, "Invalid integer encoding 0x%02X", encoding);
    return 0;
}

/* Decode the entry encoding type and data length (string length for strings,
 * number of bytes used for the integer for integer entries) encoded in 'ptr'.
 * The 'encoding' variable will hold the entry encoding, the 'lensize'
 * variable will hold the number of bytes required to encode the entry
 * length, and the 'len' variable will hold the entry length. */
#define ZIP_DECODE_LENGTH(ptr, encoding, lensize, len) do {                    \
    ZIP_ENTRY_ENCODING((ptr), (encoding));                                     \
    if ((encoding) < ZIP_STR_MASK) {                                           \
        if ((encoding) == ZIP_STR_06B) {                                       \
            (lensize) = 1;                                                     \
            (len) = (ptr)[0] & 0x3f;                                           \
        } else if ((encoding) == ZIP_STR_14B) {                                \
            (lensize) = 2;                                                     \
            (len) = (((ptr)[0] & 0x3f) << 8) | (ptr)[1];                       \
        } else if ((encoding) == ZIP_STR_32B) {                                \
            (lensize) = 5;                                                     \
            (len) = ((ptr)[1] << 24) |                                         \
                    ((ptr)[2] << 16) |                                         \
                    ((ptr)[3] <<  8) |                                         \
                    ((ptr)[4]);                                                \
        } else {                                                               \
            fprintf(stderr, "Invalid string encoding 0x%02X", (encoding));     \
        }                                                                      \
    } else {                                                                   \
        (lensize) = 1;                                                         \
        (len) = zipIntSize(encoding);                                          \
    }                                                                          \
} while(0);

#define ZIP_IS_END(entry) ((uint8_t)entry[0] == ZIP_END)

/* Return the number of bytes used to encode the length of the previous
 * entry. The length is returned by setting the var 'prevlensize'. */
#define ZIP_DECODE_PREVLENSIZE(ptr, prevlensize) do {                          \
    if ((ptr)[0] < ZIP_BIG_PREVLEN) {                                          \
        (prevlensize) = 1;                                                     \
    } else {                                                                   \
        (prevlensize) = 5;                                                     \
    }                                                                          \
} while(0);

/* Return the length of the previous element, and the number of bytes that
 * are used in order to encode the previous element length.
 * 'ptr' must point to the prevlen prefix of an entry (that encodes the
 * length of the previous entry in order to navigate the elements backward).
 * The length of the previous entry is stored in 'prevlen', the number of
 * bytes needed to encode the previous entry length are stored in
 * 'prevlensize'. */
#define ZIP_DECODE_PREVLEN(ptr, prevlensize, prevlen) do {                     \
    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize);                                  \
    if ((prevlensize) == 1) {                                                  \
        (prevlen) = (ptr)[0];                                                  \
    } else if ((prevlensize) == 5) {                                           \
        assert(sizeof((prevlen)) == 4);                                        \
        memcpy(&(prevlen), ((char*)(ptr)) + 1, 4);                             \
        memrev32ifbe(&prevlen);                                                \
    }                                                                          \
} while(0);

/* Return the total number of bytes used by the entry pointed to by 'p'. */
unsigned int
zipRawEntryLength(unsigned char *p) {
    unsigned int prevlensize, encoding, lensize, len;
    ZIP_DECODE_PREVLENSIZE(p, prevlensize);
    ZIP_DECODE_LENGTH(p + prevlensize, encoding, lensize, len);
    return prevlensize + lensize + len;
}

/* Return a struct with all information about an entry. */
void 
zipEntry(unsigned char *p, zlentry *e) {

    ZIP_DECODE_PREVLEN(p, e->prevrawlensize, e->prevrawlen);
    ZIP_DECODE_LENGTH(p + e->prevrawlensize, e->encoding, e->lensize, e->len);
    e->headersize = e->prevrawlensize + e->lensize;
    e->p = p;
}

/* Read integer encoded as 'encoding' from 'p' */
int64_t 
zipLoadInteger(unsigned char *p, unsigned char encoding) {
    int16_t i16;
    int32_t i32;
    int64_t i64, ret = 0;
    if (encoding == ZIP_INT_8B) {
        ret = ((int8_t*)p)[0];
    } else if (encoding == ZIP_INT_16B) {
        nx_memcpy(&i16,p,sizeof(i16));
        memrev16ifbe(&i16);
        ret = i16;
    } else if (encoding == ZIP_INT_32B) {
        nx_memcpy(&i32,p,sizeof(i32));
        memrev32ifbe(&i32);
        ret = i32;
    } else if (encoding == ZIP_INT_24B) {
        i32 = 0;
        nx_memcpy(((uint8_t*)&i32)+1,p,sizeof(i32)-sizeof(uint8_t));
        memrev32ifbe(&i32);
        ret = i32>>8;
    } else if (encoding == ZIP_INT_64B) {
        nx_memcpy(&i64,p,sizeof(i64));
        memrev64ifbe(&i64);
        ret = i64;
    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {
        ret = (encoding & ZIP_INT_IMM_MASK)-1;
    } else {
        assert(NULL);
    }
    return ret;
}

/* Return pointer to next entry in ziplist.
 *
 * zl is the pointer to the ziplist
 * p is the pointer to the current element
 *
 * The element after 'p' is returned, otherwise NULL if we are at the end. */
unsigned char *
ziplistNext(const char *zl, unsigned char *p) {
    ((void) zl);

    /* "p" could be equal to ZIP_END, caused by ziplistDelete,
     * and we should return NULL. Otherwise, we should return NULL
     * when the *next* element is ZIP_END (there is no next entry). */
    if (p[0] == ZIP_END) {
        return NULL;
    }

    p += zipRawEntryLength(p);
    if (p[0] == ZIP_END) {
        return NULL;
    }

    return p;
}

/* Get entry pointed to by 'p' and store in '**sstr, *slen'.
 * Return 0 if error, raw entry length otherwise. */
size_t
ziplistGet(unsigned char *p, char **sstr, size_t *slen, nx_pool_t *pool) {
    zlentry entry;
    char *s;
    int64_t v64;

    zipEntry(p, &entry);

    if (ZIP_IS_STR(entry.encoding)) {
        *slen = entry.len;
        s = p + entry.headersize;

        *sstr = nx_palloc(pool, (*slen) + 1);
        nx_memcpy(*sstr, s, (*slen));
        (*sstr)[*slen] = '\0';
    }
    else {
        v64 = zipLoadInteger(p + entry.headersize, entry.encoding);
        *sstr = nx_palloc(pool, 30);
        o_snprintf(*sstr, 30, "%lld", v64);
        *slen = nx_strlen(*sstr);
    }
    return entry.headersize + entry.len;
}

void
load_ziplist_list_or_set (rdb_parser_t *rp, const char *zl, rdb_kv_chain_t **vall, size_t *size)
{
    size_t n;
    unsigned char *p;
    size_t sz;

    rdb_kv_chain_t *ln, **ll;
    char *sstr;
    size_t slen;

    ll = vall;
    sz = 0;

    p = (unsigned char *)ZIPLIST_ENTRY_HEAD(zl);
    while (p && !ZIP_IS_END(p)) {
        n = ziplistGet(p, &sstr, &slen, rp->o_pool);
		/* p = ziplistNext(zl, p); */
		p += n;

        ln = alloc_rdb_kv_chain_link(rp->o_pool, ll);
        nx_str_set2(&ln->kv->val, sstr, slen);
        ll = &ln;

        ++sz;
    }

    (*size) = sz;
}

void
load_ziplist_hash_or_zset(rdb_parser_t *rp, const char *zl, rdb_kv_chain_t **vall, size_t *size)
{
    size_t n;
    unsigned char *p;
    size_t sz;

    rdb_kv_chain_t *ln, **ll;
    char *skey, *sval;
    size_t klen, vlen;

    ll = vall;
    sz = 0;

    p = (unsigned char *)ZIPLIST_ENTRY_HEAD(zl);
	while (p && !ZIP_IS_END(p)) {
        /* key */
		n = ziplistGet(p, &skey, &klen, rp->o_pool);
		assert(n > 0);
		/* p = ziplistNext(zl, p); */
		p += n;

        /* val */
		n = ziplistGet(p, &sval, &vlen, rp->o_pool);
		/* p = ziplistNext(zl, p); */
		p += n;

        ln = alloc_rdb_kv_chain_link(rp->o_pool, ll);
        nx_str_set2(&ln->kv->key, skey, klen);
        nx_str_set2(&ln->kv->val, sval, vlen);
        ll = &ln;

        ++sz;
    }

    (*size) = sz;
}

void
ziplist_dump(rdb_parser_t *rp, const char *zl)
{
    size_t n;
    unsigned char *p;
    uint32_t i = 0, bytes, len;
    char *sstr;
    size_t slen;

    bytes = ZIPLIST_BYTES(zl);
    memrev32ifbe(bytes);
    printf("ziplist { \n");
    printf("bytes: %u\n", bytes);
    printf("len: %u\n", ZIPLIST_LENGTH(zl));

    len = ZIPLIST_LENGTH(zl);

    p = (unsigned char *)ZIPLIST_ENTRY_HEAD(zl);
	while (p && !ZIP_IS_END(p)) {
		n = ziplistGet(p, &sstr, &slen, rp->o_pool);
		/* p = ziplistNext(zl, p); */
		p += n;

		printf("str value: %s -- %d\n", sstr, slen);
		nx_pfree(rp->o_pool, sstr);
    }

    printf("}\n");
    if(i < (0xffff - 1) && i != len) {
        printf("====== Ziplist len error. ======\n");
        exit(1);
    }
}
