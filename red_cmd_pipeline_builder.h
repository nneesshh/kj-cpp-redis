#pragma once

#include <functional>

#include "base/nx_buf.h"
#include "base/reply_parser/reply_parser_def.h"

/* redis command pipeline */
typedef struct red_cmd_pipeline_s        red_cmd_pipeline_t;
typedef struct red_cmd_pipeline_chain_s  red_cmd_pipeline_chain_t;

class CRedisReply;
using redis_reply_cb_t = std::function<void(CRedisReply&&)>;

using dispose_cb_t = std::function<void()>;

struct red_cmd_pipeline_s {
	/* pipeline state */
	enum PIPELINE_STATE {
		IDLE = 0,
		QUEUEING = 1,
		SENDING = 2,
		COMMITTING = 3,
		PROCESSING = 4,
		PROCESS_OVER = 5,
	};

	int _sn;
	std::string _commands;
	int _built_num;
	int _processed_num;
	redis_reply_cb_t _reply_cb;
	dispose_cb_t _dispose_cb;
	PIPELINE_STATE _state;
};

struct red_cmd_pipeline_chain_s {
	red_cmd_pipeline_t        *elem;
	red_cmd_pipeline_chain_t  *next;
};

/* parser */
typedef struct red_cmd_pipeline_builder_s  red_cmd_pipeline_builder_t;

struct red_cmd_pipeline_builder_s {
    nx_pool_t                 *pool;

	red_cmd_pipeline_chain_t  *cpl;
	red_cmd_pipeline_chain_t  *tail;
	size_t                     nbusy;

	red_cmd_pipeline_chain_t  *freel;
	size_t                     nfree;
};

#define FREE_RED_CMD_PIPELINE_CHAIN_LINK(__cl__, __free__)  \
    do {                                                    \
        __cl__->next = *__free__;                           \
        *__free__ = __cl__;                                 \
        reset_red_cmd_pipeline(__cl__->elem);               \
    } while(0)

red_cmd_pipeline_chain_t *    alloc_red_cmd_pipeline_chain_link(nx_pool_t *pool, red_cmd_pipeline_chain_t **ll, red_cmd_pipeline_chain_t **free);

red_cmd_pipeline_builder_t *  create_red_cmd_pipeline_builder();
void                          destroy_red_cmd_pipeline_builder(red_cmd_pipeline_builder_t *cpb);
void                          red_cmd_pipeline_builder_update(red_cmd_pipeline_builder_t *cpb);

red_cmd_pipeline_t *          new_red_cmd_pipeline();
void                          delete_red_cmd_pipeline(red_cmd_pipeline_t *rcp);
void                          reset_red_cmd_pipeline(red_cmd_pipeline_t *rcp);

/* EOF */