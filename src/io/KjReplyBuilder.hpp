#pragma once
//------------------------------------------------------------------------------
/**
@class KjReplyBuilder

(C) 2016 n.lee
*/

#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <functional>

#ifdef __cplusplus 
extern "C" {
#endif 

#include "base/reply_parser/reply_parser.h"
#include "base/reply_parser/reply_builder.h"
#include "base/reply_parser/reply_build_helper.h"

#ifdef __cplusplus 
}
#endif 

#include "base/RedisReply.h"
#include "KjSimpleThreadIoContext.hpp"

class KjReplyBuilder {
public:
	//! ctor & dtor
	KjReplyBuilder();
	~KjReplyBuilder();

	//! copy ctor & assignment operator
	KjReplyBuilder(const KjReplyBuilder&) = delete;
	KjReplyBuilder& operator=(const KjReplyBuilder&) = delete;

public:
	//! add data to reply builder
	KjReplyBuilder& ProcessInput(bip_buf_t& bb) {

		while (bip_buf_get_committed_size(&bb) > 0
			&& BuildReply(bb));

		return *this;
	}

	//! add data to reply builder
	KjReplyBuilder& operator<<(bip_buf_t& bb) {
		return ProcessInput(bb);
	}

	//! returns whether a reply is available
	bool IsReplyAvailable() const {
		return _available_replies.size() > 0;
	}

	void Reset() {
		reset_reply_parser(_parser);
		_available_replies.clear();
	}

private:
	//! build reply. Return whether the reply has been fully built or not
	bool BuildReply(bip_buf_t& bb);

	void SetReply(CRedisReply& reply, redis_reply_t *r);
	void SetArrayReply(CRedisReply& reply, uint8_t arrtype, nx_array_t *arrval);

public:
	void PushReply(redis_reply_t *r);
	CRedisReply PopReply();

private:
	redis_reply_parser_t *_parser;

	std::deque<CRedisReply> _available_replies;
};

/* EOF */