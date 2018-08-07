#include "nx_buf.h"

#include <stdlib.h>
#include <memory.h>


static INLINE void *nx_palloc_small(nx_pool_t *pool, size_t size, u_int align);
static void *nx_palloc_block(nx_pool_t *pool, size_t size);
static void *nx_palloc_large(nx_pool_t *pool, size_t size);


void *
nx_alloc(size_t size)
{
    void  *p;

    p = malloc(size);
    if (p == NULL) {
        return NULL;
    }
    return p;
}


void *
nx_calloc(size_t size)
{
    void  *p;

    p = nx_alloc(size);

    if (p) {
        nx_memzero(p, size);
    }

    return p;
}


nx_pool_t *
nx_create_pool(size_t size)
{
    nx_pool_t  *p;

    p = nx_memalign(NX_POOL_ALIGNMENT, size);
    if (p == NULL) {
        return NULL;
    }

    p->d.last = (u_char *)p + sizeof(nx_pool_t);
    p->d.end = (u_char *)p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(nx_pool_t);
    p->max = (size < NX_MAX_ALLOC_FROM_POOL) ? size : NX_MAX_ALLOC_FROM_POOL;

    p->current = p;
    p->chain = NULL;
    p->large = NULL;
    p->cleanup = NULL;

    return p;
}


void
nx_destroy_pool(nx_pool_t *pool)
{
    nx_pool_t          *p, *n;
    nx_pool_large_t    *l;
    nx_pool_cleanup_t  *c;

    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            c->handler(c->data);
        }
    }

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            nx_free(l->alloc);
        }
    }

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        nx_free(p);

        if (n == NULL) {
            break;
        }
    }
}


void
nx_reset_pool(nx_pool_t *pool)
{
    nx_pool_t        *p;
    nx_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            nx_free(l->alloc);
        }
    }

    pool->d.last = (u_char *)pool + sizeof(nx_pool_t);
    pool->d.failed = 0;

    for (p = pool->d.next; p; p = p->d.next) {
        p->d.last = (u_char *)p + sizeof(nx_pool_data_t);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->chain = NULL;
    pool->large = NULL;
}


void *
nx_palloc(nx_pool_t *pool, size_t size)
{
#if !(NX_DEBUG_PALLOC)
    if (size <= pool->max) {
        return nx_palloc_small(pool, size, 1);
    }
#endif

    return nx_palloc_large(pool, size);
}


void *
nx_pnalloc(nx_pool_t *pool, size_t size)
{
#if !(NX_DEBUG_PALLOC)
    if (size <= pool->max) {
        return nx_palloc_small(pool, size, 0);
    }
#endif

    return nx_palloc_large(pool, size);
}


static INLINE void *
nx_palloc_small(nx_pool_t *pool, size_t size, u_int align)
{
    u_char     *m;
    nx_pool_t  *p;

    p = pool->current;

    do {
        m = p->d.last;

        if (align) {
            m = nx_align_ptr(m, NX_ALIGNMENT);
        }

        if ((size_t)(p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }

        p = p->d.next;

    } while (p);

    return nx_palloc_block(pool, size);
}


static void *
nx_palloc_block(nx_pool_t *pool, size_t size)
{
    u_char     *m;
    size_t      psize;
    nx_pool_t  *p, *_new;

    psize = (size_t)(pool->d.end - (u_char *)pool);

    m = nx_memalign(NX_POOL_ALIGNMENT, psize);
    if (m == NULL) {
        return NULL;
    }

    _new = (nx_pool_t *)m;

    _new->d.end = m + psize;
    _new->d.next = NULL;
    _new->d.failed = 0;

    m += sizeof(nx_pool_data_t);
    m = nx_align_ptr(m, NX_ALIGNMENT);
    _new->d.last = m + size;

    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;
        }
    }

    p->d.next = _new;

    return m;
}


static void *
nx_palloc_large(nx_pool_t *pool, size_t size)
{
    void             *p;
    u_int             n;
    nx_pool_large_t  *large;

    p = nx_alloc(size);
    if (p == NULL) {
        return NULL;
    }

    n = 0;

    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }

        if (n++ > 3) {
            break;
        }
    }

    large = nx_palloc_small(pool, sizeof(nx_pool_large_t), 1);
    if (large == NULL) {
        nx_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


void *
nx_pmemalign(nx_pool_t *pool, size_t size, size_t alignment)
{
    void             *p;
    nx_pool_large_t  *large;

    p = nx_memalign(alignment, size);
    if (p == NULL) {
        return NULL;
    }

    large = nx_palloc_small(pool, sizeof(nx_pool_large_t), 1);
    if (large == NULL) {
        nx_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


int
nx_pfree(nx_pool_t *pool, void *p)
{
    nx_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            nx_free(l->alloc);
            l->alloc = NULL;

            return NX_OK;
        }
    }

    return NX_DECLINED;
}


void *
nx_pcalloc(nx_pool_t *pool, size_t size)
{
    void  *p;

    p = nx_palloc(pool, size);
    if (p) {
        nx_memzero(p, size);
    }

    return p;
}


nx_pool_cleanup_t *
nx_pool_cleanup_add(nx_pool_t *p, size_t size)
{
    nx_pool_cleanup_t  *c;

    c = nx_palloc(p, sizeof(nx_pool_cleanup_t));
    if (c == NULL) {
        return NULL;
    }

    if (size) {
        c->data = nx_palloc(p, size);
        if (c->data == NULL) {
            return NULL;
        }

    }
    else {
        c->data = NULL;
    }

    c->handler = NULL;
    c->next = p->cleanup;

    p->cleanup = c;

    return c;
}


nx_buf_t *
nx_create_temp_buf(nx_pool_t *pool, size_t size)
{
    nx_buf_t  *b;

    b = nx_calloc_buf(pool);
    if (b == NULL) {
        return NULL;
    }

    b->start = nx_palloc(pool, size);
    if (b->start == NULL) {
        return NULL;
    }

    /*
     * set by x_calloc_buf():
     *
     *     b->file_pos = 0;
     *     b->file_last = 0;
     *     b->file = NULL;
     *     b->shadow = NULL;
     *     b->tag = 0;
     *     and flags
     */

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;

    return b;
}


nx_chain_t *
nx_alloc_chain_link(nx_pool_t *pool)
{
    nx_chain_t  *cl;

    cl = pool->chain;

    if (cl) {
        pool->chain = cl->next;
        return cl;
    }

    cl = nx_palloc(pool, sizeof(nx_chain_t));
    if (cl == NULL) {
        return NULL;
    }

    return cl;
}


nx_chain_t *
nx_create_chain_of_bufs(nx_pool_t *pool, nx_bufs_t *bufs)
{
    u_char      *p;
    int          i;
    nx_buf_t    *b;
    nx_chain_t  *chain, *cl, **ll;

    p = nx_palloc(pool, bufs->num * bufs->size);
    if (p == NULL) {
        return NULL;
    }

    ll = &chain;

    for (i = 0; i < bufs->num; i++) {

        b = nx_calloc_buf(pool);
        if (b == NULL) {
            return NULL;
        }

        /*
         * set by x_calloc_buf():
         *
         *     b->file_pos = 0;
         *     b->file_last = 0;
         *     b->file = NULL;
         *     b->shadow = NULL;
         *     b->tag = 0;
         *     and flags
         *
         */

        b->pos = p;
        b->last = p;

        b->start = p;
        p += bufs->size;
        b->end = p;

        cl = nx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NULL;
        }

        cl->buf = b;
        *ll = cl;
        ll = &cl->next;
    }

    *ll = NULL;

    return chain;
}


int
nx_chain_add_copy(nx_pool_t *pool, nx_chain_t **chain, nx_chain_t *in)
{
    nx_chain_t  *cl, **ll;

    ll = chain;

    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        cl = nx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NX_ERROR;
        }

        cl->buf = in->buf;
        *ll = cl;
        ll = &cl->next;
        in = in->next;
    }

    *ll = NULL;

    return NX_OK;
}


nx_chain_t *
nx_chain_get_free_buf(nx_pool_t *p, nx_chain_t **free)
{
    nx_chain_t  *cl;

    if (*free) {
        cl = *free;
        *free = cl->next;
        cl->next = NULL;
        return cl;
    }

    cl = nx_alloc_chain_link(p);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = nx_calloc_buf(p);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    return cl;
}


void
nx_chain_update_chains(nx_pool_t *p, nx_chain_t **free, nx_chain_t **busy, nx_chain_t **out)
{
    nx_chain_t  *cl;

    if (*out) {
        if (*busy == NULL) {
            *busy = *out;

        } else {
            for (cl = *busy; cl->next; cl = cl->next) { /* void */ }

            cl->next = *out;
        }

        *out = NULL;
    }

    while (*busy) {
        cl = *busy;

        if (nx_buf_size(cl->buf) != 0) {
            break;
        }

        cl->buf->pos = cl->buf->start;
        cl->buf->last = cl->buf->start;

        *busy = cl->next;
        cl->next = *free;
        *free = cl;
    }
}


nx_chain_t *
nx_chain_update_sent(nx_chain_t *in, u_long sent)
{
    u_long  size;

    for ( /* void */ ; in; in = in->next) {

        if (sent == 0) {
            break;
        }

        size = nx_buf_size(in->buf);

        if (sent >= size) {
            sent -= size;

            in->buf->pos = in->buf->last;

            continue;
        }

        in->buf->pos += (size_t) sent;

        break;
    }

    return in;
}
