#include "rdb_object_builder.h"

rdb_object_builder_t *
stack_push_object_builder(rdb_parser_t *rp)
{
    rdb_object_builder_t *ob;

    ob = nx_array_push(rp->stack_ob);
    nx_memzero(ob, sizeof(rdb_object_builder_t));
    return ob;
}

void
stack_pop_object_builder(rdb_parser_t *rp)
{
    rdb_object_builder_t *ob;

    ob = nx_array_pop(rp->stack_ob);
    nx_memzero(ob, sizeof(rdb_object_builder_t));
}

rdb_object_builder_t *
stack_alloc_object_builder(rdb_parser_t *rp, uint8_t depth)
{
    rdb_object_builder_t *ob;

    ob = stack_object_builder_at(rp, depth);
    if (NULL == ob) {
        ob = stack_push_object_builder(rp);
        ob->depth = depth;
    }
    return ob;
}

rdb_object_builder_t *
stack_object_builder_at(rdb_parser_t *rp, uint8_t depth)
{
    return nx_array_at(rp->stack_ob, depth);
}

rdb_object_builder_t *
stack_object_builder_top(rdb_parser_t *rp)
{
    return nx_array_top(rp->stack_ob);
}