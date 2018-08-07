#include "reply_parser.h"
#include <time.h>
#include <assert.h>

#ifdef _DEBUG
#include <vld.h>
#endif

#include "reply_builder.h"
#include "r_build_factory.h"

enum BUILD_REPLY_TAG {
    BUILD_REPLY_IDLE = 0,
    BUILD_REPLY_TYPE,
    BUILD_REPLY_CONTENT,
    BUILD_REPLY_OVER,
};

static int __build_reply_type(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb);
static int __build_reply_content(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb);

static int
__build_reply_type(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb)
{
    size_t n;
    uint8_t leading, type;

    redis_reply_t *r;

    if ((n = redis_reply_read_leading(rrp, bb, &leading)) == 0)
        return RRB_ERROR_PREMATURE;

    type = REDIS_REPLY_LEADING_TYPE_UNKNOWN;

    switch (leading) {
    case '+': /* simple string */
        type = REDIS_REPLY_LEADING_TYPE_SIMPLE_STRING;
        break;

    case '-': /* error string */
        type = REDIS_REPLY_LEADING_TYPE_ERROR_STRING;
        break;

    case ':': /* integer */
        type = REDIS_REPLY_LEADING_TYPE_INTEGER;
        break;

    case '$': /* bulk string */
        type = REDIS_REPLY_LEADING_TYPE_BULK_STRING;
        break;

    case '*': /* array */
        type = REDIS_REPLY_LEADING_TYPE_ARRAY;
        break;

    default:
        return RRB_ERROR_INVALID_LEADING_CHAR;
    }

    /* reply type */
    r = rrp->r;
    r->type = type;

    bip_buf_decommit(bb, n);
    rrp->parsed += n;

    /* next state */
    rrb->state = BUILD_REPLY_CONTENT;
    return RRB_AGAIN;
}

static int
__build_reply_content(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb)
{
    redis_reply_t *r;

    r = rrp->r;

    switch (r->type) {
    case REDIS_REPLY_LEADING_TYPE_SIMPLE_STRING:
        return r_build_simple_string(rrp, rrb, bb, r);

    case REDIS_REPLY_LEADING_TYPE_ERROR_STRING:
        return r_build_simple_string(rrp, rrb, bb, r);

    case REDIS_REPLY_LEADING_TYPE_INTEGER:
        return r_build_integer(rrp, rrb, bb, &r->intval);

    case REDIS_REPLY_LEADING_TYPE_BULK_STRING:
        return r_build_bulk_string(rrp, rrb, bb, r);

    case REDIS_REPLY_LEADING_TYPE_ARRAY:
        return r_build_array(rrp, rrb, bb, r);

    default:
        break;
    }

    return RRB_ERROR_INVALID_LEADING_CHAR;
}

int
r_build_reply(redis_reply_parser_t *rrp, bip_buf_t *bb)
{
    int rc = 0;
    redis_reply_builder_t *rrb;
    int depth = 0;

    /* main builder */
    RRB_LOOP_BEGIN(rrp, rrb, depth)
    {
        /* main process */
        switch (rrb->state) {
        case BUILD_REPLY_IDLE:
        case BUILD_REPLY_TYPE:
            rc = __build_reply_type(rrp, rrb, bb);
            break;

        case BUILD_REPLY_CONTENT:
            rc = __build_reply_content(rrp, rrb, bb);
            break;

        default:
            rc = RRB_ERROR_INVALID_RRB_STATE;
            break;
        }

    }
    RRB_LOOP_END(rrp, rc)
    return rc;
}
