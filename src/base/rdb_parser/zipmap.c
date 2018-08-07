#include "zipmap.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../endian.h"
#include "build_helper.h"

#define ZM_END 0xff
#define ZM_BIGLEN 0xfd
#define ZM_IS_END(entry) ((uint8_t)entry[0] == ZM_END)

static uint32_t
__zipmap_entry_len_size(const char *entry)
{
    uint32_t size = 0;
    if ((uint8_t)entry[0] < ZM_BIGLEN) {
        size = 1;
    } else if ((uint8_t)entry[0] == ZM_BIGLEN) {
        size = 5;
    }

    return size;
}

static uint32_t
__zipmap_entry_strlen (const char *entry)
{
    uint32_t entry_len_size;
    uint32_t slen;

    entry_len_size = __zipmap_entry_len_size(entry);
    if (entry_len_size == 1) {
        return (uint8_t)entry[0];
    } else if (entry_len_size == 5) {
        slen = 0;
        nx_memcpy(&slen, entry + 1, 4);
        memrev32ifbe(&slen);
        return slen;
    } else {
        return 0;
    }
}

static uint32_t
__zipmap_entry_len (const char *entry)
{
    return  __zipmap_entry_len_size(entry) + __zipmap_entry_strlen(entry);
}

void
load_zipmap(rdb_parser_t *rp, const char *zm, rdb_kv_chain_t **vall, size_t *size)
{
    uint32_t len = 0;
    int klen, vlen;
    char *key, *val;

    rdb_kv_chain_t *ln, **ll;

    ll = vall;

    ++zm;
    while (!ZM_IS_END(zm)) {

        /* key */
        klen = __zipmap_entry_strlen(zm);
        key = nx_palloc(rp->o_pool, klen + 1);
        nx_memcpy(key, zm + __zipmap_entry_len_size(zm), klen);
        key[klen] = '\0';
        zm += __zipmap_entry_len(zm);

        /* value */
        vlen = __zipmap_entry_strlen(zm);
        val = nx_palloc(rp->o_pool, vlen + 1);
        nx_memcpy(val, zm + __zipmap_entry_len_size(zm) + 1, vlen);
        val[vlen] = '\0';
        zm += __zipmap_entry_len(zm) + 1;

        ln = alloc_rdb_kv_chain_link(rp->o_pool, ll);
        nx_str_set2(&ln->kv->key, key, klen);
        nx_str_set2(&ln->kv->val, val, vlen);
        ll = &ln;

        ++len;
    }

    (*size) = len;
}

void
zipmap_dump(rdb_parser_t *rp, const char *s)
{
    int i = 0, len, klen, vlen;
    char *key, *val;

    len = (uint8_t) s[0];
    ++s;
    while (!ZM_IS_END(s)) {
        // key
        klen = __zipmap_entry_strlen(s);
        key = nx_palloc(rp->o_pool, klen + 1);
        nx_memcpy(key, s + __zipmap_entry_len_size(s), klen);
        key[klen] = '\0';
        s += __zipmap_entry_len(s);

        // value
        vlen = __zipmap_entry_strlen(s);
        val = nx_palloc(rp->o_pool, vlen + 1);
        val[vlen] = '\0';
        nx_memcpy(val, s + __zipmap_entry_len_size(s) + 1, vlen);
        s += __zipmap_entry_len(s) + 1;

        printf(" key is %s, value is %s\n", key, val);

        i+=2;
    }

    if(len >=254 && i == len) {
        printf("===== zipmap len error.\n");
        exit(1);
    }
}
