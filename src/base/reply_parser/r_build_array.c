#include "r_build_factory.h"

enum BUILD_ARRAY_TAG {
    BUILD_ARRAY_IDLE = 0,
    BUILD_ARRAY_LEN,
    BUILD_ARRAY_ELEMENT_TYPE,
    BUILD_ARRAY_ELEMENT,
};

static int __build_array_len(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);
static int __build_array_element_type(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);
static int __build_array_element(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r);

static int
__build_array_len(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    int rc;
    int64_t len;

    rc = r_build_integer(rrp, rrb, bb, &len);

    if (rc == RRB_OVER) {
        if (len > 0) {
            
            rrb->store_len = 0; /* string len */
            rrb->len = (uint32_t)len; /* array len */

            /* pre-alloc */
            r->arrval = nx_array_create(rrp->r_pool, rrb->len, sizeof(redis_reply_t));

            /* next state */
            rrb->state = BUILD_ARRAY_ELEMENT_TYPE;
        }
        else {
            /* "len==0" means empty, format: "*0\r\n" */
            /* "len==-1" means null, format: "*-1\r\n" */
            r->is_null = (len < 0);
            r->elemtype = 0;
            r->arrval = NULL;
            return RRB_OVER;
        }

        /* next state */
        rrb->state = BUILD_ARRAY_ELEMENT_TYPE;
        return RRB_AGAIN;
    }

    return rc;
}

static int
__build_array_element_type(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    size_t n;
    uint8_t leading, type;

    redis_reply_chain_t **ll;

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

    r->elemtype = type;

    bip_buf_decommit(bb, n);
    rrp->parsed += n;

    /* next state */
    rrb->state = BUILD_ARRAY_ELEMENT;
    return RRB_AGAIN;
}

static int
__build_array_element(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    int rc;

    redis_reply_t *r2;

    /* alloc child-reply in array */
    r2 = nx_array_at(r->arrval, rrb->arridx);
    if (NULL == r2) {
        r2 = nx_array_push(r->arrval);
        redis_reply_init(r2);
        r2->type = r->elemtype;
    }

    switch (r2->type) {
    case REDIS_REPLY_LEADING_TYPE_SIMPLE_STRING:
        rc = r_build_simple_string(rrp, rrb, bb, r2);
        break;

    case REDIS_REPLY_LEADING_TYPE_ERROR_STRING:
        rc = r_build_simple_string(rrp, rrb, bb, r2);
        break;

    case REDIS_REPLY_LEADING_TYPE_INTEGER:
        rc = r_build_integer(rrp, rrb, bb, &r2->intval);
        break;

    case REDIS_REPLY_LEADING_TYPE_BULK_STRING:
        rc = r_build_bulk_string(rrp, rrb, bb, r2);
        break;

    case REDIS_REPLY_LEADING_TYPE_ARRAY:
        rc = r_build_array(rrp, rrb, bb, r2);
        break;

    default:
        return RRB_ERROR_INVALID_LEADING_CHAR;
    }

    if (rc == RRB_OVER) {
        /* next array element */
        ++rrb->arridx;

        if (rrb->arridx + 1 <= rrb->len) {
            /* next state */
            rrb->state = BUILD_ARRAY_ELEMENT_TYPE;
            return RRB_AGAIN;
        }
    }

    return rc;
}

int
r_build_array(redis_reply_parser_t *rrp, redis_reply_builder_t *rrb, bip_buf_t *bb, redis_reply_t *r)
{
    int rc = 0;
    redis_reply_builder_t *sub_rrb;
    int depth = rrb->depth + 1;

    /* sub builder */
    RRB_LOOP_BEGIN(rrp, sub_rrb, depth)
    {
        /* sub process */
        switch (sub_rrb->state) {
        case BUILD_ARRAY_IDLE:
        case BUILD_ARRAY_LEN:
            rc = __build_array_len(rrp, sub_rrb, bb, r);
            break;

        case BUILD_ARRAY_ELEMENT_TYPE:
            rc = __build_array_element_type(rrp, sub_rrb, bb, r);
            break;

        case BUILD_ARRAY_ELEMENT:
            rc = __build_array_element(rrp, sub_rrb, bb, r);
            break;

        default:
            rc = RRB_ERROR_INVALID_RRB_STATE;
            break;
        }

    }
    RRB_LOOP_END(rrp, rc)
    return rc;
}
