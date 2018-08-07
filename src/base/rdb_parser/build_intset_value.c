#include "build_factory.h"

#include <string.h>

#include "intset.h"

int
build_intset_value(rdb_parser_t *rp, rdb_object_builder_t *ob, bip_buf_t *bb, rdb_kv_chain_t **vall, size_t *size)
{
    int rc = 0;
    uint32_t i;
    int64_t v64;
    char *s64;
    intset_t *is;

    rdb_kv_chain_t *ln, **ll;

    /* val */
    rc = build_string_value(rp, ob, bb, &ob->tmp_val);

    /* over */
    if (rc == OB_OVER) {
        ll = vall;
        is = (intset_t*)ob->tmp_val.data;

        for (i = 0; i < is->length; ++i) {
            intset_get(is, i, &v64);

            ln = alloc_rdb_kv_chain_link(rp->o_pool, ll);

            s64 = nx_palloc(rp->o_pool, 30);
            o_snprintf(s64, 30, "%lld", v64);
            nx_str_set2(&ln->kv->val, s64, nx_strlen(s64));

            ll = &ln;
        }

        (*size) = is->length;

        nx_pfree(rp->o_pool, ob->tmp_val.data);
        nx_str_null(&ob->tmp_val);
    }
    return rc;
}
