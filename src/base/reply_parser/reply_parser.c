#include "reply_parser.h"
#include <time.h>
#include <assert.h>

#ifdef _MSC_VER
# ifdef _DEBUG
#  include <vld.h>
# endif
#endif

#include "reply_builder.h"
#include "r_build_factory.h"

#define REDIS_PARSER_INPUT_BIPBUF_SIZE  4096
#define REDIS_PARSER_POOL_SIZE          1024
#define REDIS_PARSER_REPLY_POOL_SIZE    4096

enum PARSE_REDIS_REPLY_TAG {
    PARSE_REDIS_REPLY_IDLE = 0,
    PARSE_REDIS_REPLY_RUNNING,
    PARSE_REDIS_REPLY_OVER,
};

redis_reply_parser_t *
create_reply_parser(func_process_redis_reply cb, void *payload)
{
    redis_reply_parser_t *rrp = nx_alloc(sizeof(redis_reply_parser_t));
    nx_memzero(rrp, sizeof(redis_reply_parser_t));

    rrp->version = 6;
    rrp->parsed = 0;
    rrp->state = PARSE_REDIS_REPLY_IDLE;

    rrp->in_bb = bip_buf_create(REDIS_PARSER_INPUT_BIPBUF_SIZE);
    rrp->pool = nx_create_pool(REDIS_PARSER_POOL_SIZE);

    rrp->stack_rrb = nx_array_create(rrp->pool, 16, sizeof(redis_reply_builder_t));

    rrp->r = nx_palloc(rrp->pool, sizeof(redis_reply_t));
    redis_reply_init(rrp->r);

    rrp->r_pool = nx_create_pool(REDIS_PARSER_REPLY_POOL_SIZE);
    rrp->r_cb = cb;
    rrp->r_payload = payload;

    return rrp;
}

void
destroy_reply_parser(redis_reply_parser_t *rrp)
{
    bip_buf_destroy(rrp->in_bb);
    nx_destroy_pool(rrp->r_pool);
    nx_destroy_pool(rrp->pool);
    nx_free(rrp);
}

void
reset_reply_parser(redis_reply_parser_t *rrp)
{
    nx_pool_t  *p, *tmp;

    rrp->parsed = 0;
    rrp->state = PARSE_REDIS_REPLY_IDLE;

    /* reset and shrink reply pool*/
    nx_reset_pool(rrp->r_pool);
    p = rrp->r_pool->d.next;
    while (p) {
        tmp = p->d.next;
        nx_free(p);
        p = tmp;
    }
    rrp->r_pool->d.next = NULL;

    /* reset and shrink parser pool*/
    nx_reset_pool(rrp->pool);
    p = rrp->pool->d.next;
    while(p) {
        tmp = p->d.next;
        nx_free(p);
        p = tmp;
    }
    rrp->pool->d.next = NULL;

    bip_buf_reset(rrp->in_bb);

    rrp->stack_rrb = nx_array_create(rrp->pool, 16, sizeof(redis_reply_builder_t));

    rrp->r = nx_palloc(rrp->pool, sizeof(redis_reply_t));
    redis_reply_init(rrp->r);
}

int
redis_reply_parse_once(redis_reply_parser_t *rrp, bip_buf_t *bb)
{
    int rc;

    rc = RRB_AGAIN;

    while (rrp->state != PARSE_REDIS_REPLY_OVER
        && (rc == RRB_AGAIN || rc == RRB_OVER)) {

        switch (rrp->state) {
        case PARSE_REDIS_REPLY_IDLE:
        case PARSE_REDIS_REPLY_RUNNING:
            if ((rc = r_build_reply(rrp, bb)) == RRB_OVER) {
                /* process reply */
                if (0 == rrp->r_cb(rrp->r, rrp->r_payload)) {
                    redis_reply_clear(rrp);
                }
                else {
                    rc = RRB_ABORT;
                    rrp->state = PARSE_REDIS_REPLY_OVER;
                }
            }
            break;

        default:
            break;
        }
    }
    return rc;
}
