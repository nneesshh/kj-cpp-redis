#pragma once

#include "nx_buf.h"


typedef struct {
    void       *elts;
    int         nelts;
    size_t      size;
    u_int       nalloc;
    nx_pool_t  *pool;
} nx_array_t;


nx_array_t * nx_array_create(nx_pool_t *p, u_int n, size_t size);
void         nx_array_destroy(nx_array_t *a);
void *       nx_array_push(nx_array_t *a);
void *       nx_array_push_n(nx_array_t *a, u_int n);
void *       nx_array_pop(nx_array_t *a);
void *       nx_array_pop_all(nx_array_t *a);
void *       nx_array_top(nx_array_t *a);
void *       nx_array_at(nx_array_t *a, int index);

static INLINE int
nx_array_init(nx_array_t *array, nx_pool_t *pool, u_int n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = nx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NX_ERROR;
    }

    return NX_OK;
}

/* EOF */