//------------------------------------------------------------------------------
//  KjReplyBuilder.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjReplyBuilder.hpp"

#include <assert.h>
#include "base/RedisError.h"

static int
on_build_reply(redis_reply_t *r, void *payload) {
	KjReplyBuilder *builder = static_cast<KjReplyBuilder *>(payload);
	builder->PushReply(r);
	return 0;
}

//------------------------------------------------------------------------------
/**

*/
KjReplyBuilder::KjReplyBuilder() {
	_parser = create_reply_parser(on_build_reply, this);
}

//------------------------------------------------------------------------------
/**

*/
KjReplyBuilder::~KjReplyBuilder() {
	destroy_reply_parser(_parser);
}

//------------------------------------------------------------------------------
/**

*/
bool
KjReplyBuilder::BuildReply(bip_buf_t& bb) {
	int rc;

	rc = redis_reply_parse_once(_parser, &bb);
	if (rc != RRB_OVER
		&& rc != RRB_AGAIN
		&& rc != RRB_ERROR_PREMATURE) {
		std::string sDesc = "\n\n\n[KjReplyBuilder::BuildReply()] errcode = ";
		sDesc += std::to_string(rc);
		sDesc += " !!!\n";
		fprintf(stderr, sDesc.c_str());
		throw CRedisError(sDesc.c_str());
	}
	// only failed when premature
	return rc != RRB_ERROR_PREMATURE;
}

//------------------------------------------------------------------------------
/**

*/
void
KjReplyBuilder::SetReply(CRedisReply& reply, redis_reply_t *r) {

	if (r->is_null) {
		reply.set();
	}
	else {
		switch (r->type) {
		case REDIS_REPLY_LEADING_TYPE_SIMPLE_STRING: {
			std::string str;
			size_t len;
			
			if (r->bytes > 0)
			str.resize(r->bytes);
			redis_reply_as_string(r, (char *)str.c_str(), &len);
			reply.set(std::move(str), CRedisReply::string_type::simple_string);
			break;
		}

		case REDIS_REPLY_LEADING_TYPE_ERROR_STRING: {
			std::string str;
			size_t len;

			str.resize(r->bytes);
			redis_reply_as_string(r, (char *)str.c_str(), &len);
			reply.set(std::move(str), CRedisReply::string_type::error);
			break;
		}

		case REDIS_REPLY_LEADING_TYPE_INTEGER: {
			reply.set(r->intval);
			break;
		}

		case REDIS_REPLY_LEADING_TYPE_BULK_STRING: {
			std::string str;
			size_t len;

			str.resize(r->bytes);
			redis_reply_as_string(r, (char *)str.c_str(), &len);
			reply.set(std::move(str), CRedisReply::string_type::bulk_string);
			break;
		}

		case REDIS_REPLY_LEADING_TYPE_ARRAY: {
			SetArrayReply(reply, r->elemtype, r->arrval);
			break;
		}

		default:
			std::string sDesc = "\n\n\n[KjReplyBuilder::SetReply()] type is invalid -- ";
			sDesc += std::to_string(r->type);
			sDesc += " !!!\n";
			fprintf(stderr, sDesc.c_str());
			throw CRedisError(sDesc.c_str());
		}
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjReplyBuilder::SetArrayReply(CRedisReply& reply, uint8_t arrtype, nx_array_t *arrval) {
	std::vector<CRedisReply> vReply;

	if (arrval && arrval->nelts > 0) {
		redis_reply_t *r;
		int i;

		vReply.reserve(arrval->nelts);

		for (i = 0; i < arrval->nelts; ++i) {
			CRedisReply reply2;
			r = (redis_reply_t *)nx_array_at(arrval, i);
			SetReply(reply2, r);
			vReply.emplace_back(reply2);
		}
	}

	//
	reply.set(std::move(vReply));
}

//------------------------------------------------------------------------------
/**

*/
void
KjReplyBuilder::PushReply(redis_reply_t *r) {
	CRedisReply reply;

	switch (r->type) {
	case REDIS_REPLY_LEADING_TYPE_SIMPLE_STRING:
	case REDIS_REPLY_LEADING_TYPE_ERROR_STRING:
	case REDIS_REPLY_LEADING_TYPE_INTEGER:
	case REDIS_REPLY_LEADING_TYPE_BULK_STRING:
		SetReply(reply, r);
		break;

	case REDIS_REPLY_LEADING_TYPE_ARRAY: {
		SetArrayReply(reply, r->elemtype, r->arrval);
		break;
	}

	default:
		std::string sError = "Invalid leading type -- ";
		sError.append(std::to_string(r->type));
		sError.append("!!!");
		reply.set(std::move(sError), CRedisReply::string_type::error);
		break;
	}

	_available_replies.emplace_back(std::move(reply));
}

//------------------------------------------------------------------------------
/**

*/
CRedisReply
KjReplyBuilder::PopReply() {
	assert(_available_replies.size() > 0);
	CRedisReply& r = _available_replies.front();
	CRedisReply reply = std::move(r);
	_available_replies.pop_front();
	return reply;
}