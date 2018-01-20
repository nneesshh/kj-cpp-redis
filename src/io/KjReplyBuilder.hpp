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

#include "KjSimpleThreadIoContext.hpp"

#include "builders/builder_iface.hpp"

namespace cpp_redis {

namespace builders {

class KjReplyBuilder {
public:
	//! ctor & dtor
	KjReplyBuilder();
	~KjReplyBuilder();

	//! copy ctor & assignment operator
	KjReplyBuilder(const KjReplyBuilder&) = delete;
	KjReplyBuilder& operator=(const KjReplyBuilder&) = delete;

	enum STATE {
		STATE_HEAD = 0,
		STATE_BODY = 1,
	};

public:
	//! add data to reply builder
	KjReplyBuilder& ProcessInput(bip_buf_t& bbuf) {

		while (bip_buf_get_committed_size(&bbuf) > 0
			&& BuildReply(bbuf));

		return *this;
	}

	//! add data to reply builder
	KjReplyBuilder& operator<<(bip_buf_t& bbuf) {
		return ProcessInput(bbuf);
	}

	//! get reply
	CRedisReply& GetFront() {
		return _available_replies.front();
	}

	//! pop reply
	void PopFront() {
		_available_replies.pop_front();
	}

	//! returns whether a reply is available
	bool IsReplyAvailable() const {
		return _available_replies.size() > 0;
	}

	void Reset() {
		_state = STATE_HEAD;
		_builder_i = nullptr;
		_available_replies.clear();
	}

private:
	//! build reply. Return whether the reply has been fully built or not
	bool BuildReply(bip_buf_t& bbuf) {
		return _state_cb[_state](bbuf);
	}

private:
	std::vector<builder_iface *> _v_builder_i;
	std::function<bool(bip_buf_t&)> _state_cb[STATE_BODY + 1];

	enum STATE _state = STATE_HEAD;
	builder_iface *_builder_i = nullptr;
	std::deque<CRedisReply> _available_replies;
};

} //! builders

} //! cpp_redis