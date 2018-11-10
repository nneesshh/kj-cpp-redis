#include "red_cmd_pipeline_builder.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define REDIS_COMMAND_PIPELINE_BUILDER_POOL_SIZE  4096
#define REDIS_COMMAND_PIPELINE_BUSY_MAX           1024

red_cmd_pipeline_chain_t *
alloc_red_cmd_pipeline_chain_link(nx_pool_t *pool, red_cmd_pipeline_chain_t **ll, red_cmd_pipeline_chain_t **free)
{
    red_cmd_pipeline_chain_t  *cl, *ln;
	red_cmd_pipeline_t *cp;

    if (*free) {
        cl = *free;
        *free = cl->next;
        cl->next = NULL;
        return cl;
    }
    else {
        cl = (red_cmd_pipeline_chain_t *)nx_palloc(pool, sizeof(red_cmd_pipeline_chain_t));
        if (cl == NULL) {
            return NULL;
        }
        nx_memzero(cl, sizeof(red_cmd_pipeline_chain_t));

		cp = new_red_cmd_pipeline();

		cl->elem = cp;
		cl->next = 0;

        /* link */
        if ((*ll) == NULL) {
            (*ll) = cl;
        }
        else {
            for (ln = (*ll); ln->next; ln = ln->next) { /* void */ }

            ln->next = cl;
        }
        return cl;
    }
}

red_cmd_pipeline_builder_t *
create_red_cmd_pipeline_builder()
{
    red_cmd_pipeline_builder_t *cpb = (red_cmd_pipeline_builder_t *)nx_alloc(sizeof(red_cmd_pipeline_builder_t));
    nx_memzero(cpb, sizeof(red_cmd_pipeline_builder_t));

    cpb->pool = nx_create_pool(REDIS_COMMAND_PIPELINE_BUILDER_POOL_SIZE);
    return cpb;
}

void
destroy_red_cmd_pipeline_builder(red_cmd_pipeline_builder_t *cpb)
{
	red_cmd_pipeline_chain_t *ln;

	for (ln = cpb->cpl; ln; ln = ln->next) {
		FREE_RED_CMD_PIPELINE_CHAIN_LINK(ln, &cpb->freel);
		--cpb->nbusy;
		++cpb->nfree;
	}
	assert(0 == cpb->nbusy);

	for (ln = cpb->freel; ln; ln = ln->next) {
		delete_red_cmd_pipeline(ln->elem);
		ln->elem = NULL;

		--cpb->nfree;
	}
	assert(0 == cpb->nfree);

    nx_destroy_pool(cpb->pool);
    nx_free(cpb);
}

void
red_cmd_pipeline_builder_update(red_cmd_pipeline_builder_t *cpb)
{
	red_cmd_pipeline_chain_t *ln;
	red_cmd_pipeline_t *cp;

	if (cpb->nbusy >= REDIS_COMMAND_PIPELINE_BUSY_MAX) {

		for (ln = cpb->cpl; ln; ln = ln->next) {
			cp = ln->elem;
			if (red_cmd_pipeline_t::PROCESS_OVER == cp->_state) {

				FREE_RED_CMD_PIPELINE_CHAIN_LINK(ln, &cpb->freel);
				--cpb->nbusy;
				++cpb->nfree;
			}
			else {
				break;
			}
		}
	}
}

red_cmd_pipeline_t *
new_red_cmd_pipeline()
{
	red_cmd_pipeline_t *cp = new red_cmd_pipeline_t;
	reset_red_cmd_pipeline(cp);
	return cp;
}

void
delete_red_cmd_pipeline(red_cmd_pipeline_t *cp)
{
	delete cp;
}

void
reset_red_cmd_pipeline(red_cmd_pipeline_t *cp)
{
	cp->_sn = 0;
	cp->_commands.reserve(1024);
	cp->_commands.resize(0);
	cp->_built_num = 0;
	cp->_processed_num = 0;
	cp->_reply_cb = NULL;
	cp->_dispose_cb = NULL;
	cp->_state = red_cmd_pipeline_t::IDLE;
}