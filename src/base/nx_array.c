#include "nx_array.h"


nx_array_t *
nx_array_create(nx_pool_t *p, u_int n, size_t size)
{
    nx_array_t *a;

    a = nx_palloc(p, sizeof(nx_array_t));
    if (a == NULL) {
        return NULL;
    }

    if (nx_array_init(a, p, n, size) != NX_OK) {
        return NULL;
    }

    return a;
}


void
nx_array_destroy(nx_array_t *a)
{
    nx_pool_t  *p;

    p = a->pool;

    if ((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }

    if ((u_char *) a + sizeof(nx_array_t) == p->d.last) {
        p->d.last = (u_char *) a;
    }
}


void *
nx_array_push(nx_array_t *a)
{
    void       *elt, *_new;
    size_t      size;
    nx_pool_t  *p;

    if (a->nelts == a->nalloc) {

        /* the array is full */

        size = a->size * a->nalloc;

        p = a->pool;

        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += a->size;
            a->nalloc++;

        } else {
            /* allocate a new array */

            _new = nx_palloc(p, 2 * size);
            if (_new == NULL) {
                return NULL;
            }

            nx_memcpy(_new, a->elts, size);
            a->elts = _new;
            a->nalloc *= 2;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}


void *
nx_array_push_n(nx_array_t *a, u_int n)
{
    void       *elt, *_new;
    size_t      size;
    u_int       nalloc;
    nx_pool_t  *p;

    size = n * a->size;

    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array */

            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            _new = nx_palloc(p, nalloc * a->size);
            if (_new == NULL) {
                return NULL;
            }

            nx_memcpy(_new, a->elts, a->nelts * a->size);
            a->elts = _new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n;

    return elt;
}


void *
nx_array_pop(nx_array_t *a)
{
    void *elt;

    elt = NULL;

    if (a->nelts > 0) {
        elt = (u_char *)a->elts + a->size * a->nelts;
        --a->nelts;
    }
    return elt;
}


void *
nx_array_pop_all(nx_array_t *a)
{
    void *elt;

    elt = (u_char *)a->elts;
    a->nelts = 0;
    return elt;
}


void *
nx_array_top(nx_array_t *a)
{
    void *elt;

    elt = NULL;

    if (a->nelts > 0) {
        elt = (u_char *)a->elts + a->size * (a->nelts-1);
    }
    return elt;
}


void *
nx_array_at(nx_array_t *a, int index)
{
    void *elt;

    elt = NULL;

    if (index < a->nelts) {
        elt = (u_char *)a->elts + a->size * index;
    }
    return elt;
}