//------------------------------------------------------------------------------
//  KjReplyBuilder.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------

#include "builders/builders_factory.hpp"

#include "KjReplyBuilder.hpp"

namespace cpp_redis {

namespace builders {

//------------------------------------------------------------------------------
/**

*/
KjReplyBuilder::KjReplyBuilder() {

	create_builders(false, _v_builder_i);

	_state_cb[STATE_HEAD] = [this](bip_buf_t& bbuf)->bool {
		auto begin_sequence = bip_buf_get_contiguous_block(&bbuf);
		_builder_i = _v_builder_i[(*begin_sequence) - KJ_REPLY_BUILDER_SATRT_CHAR];
		bip_buf_decommit(&bbuf, 1);

		*_builder_i << (bbuf);

		if (_builder_i->reply_ready()) {
			_available_replies.push_back(_builder_i->get_reply());
			_builder_i = nullptr;
			
			_state = STATE_HEAD;
			return true;
		}

		_state = STATE_BODY;
		return false;
	};

	_state_cb[STATE_BODY] = [this](bip_buf_t& bbuf)->bool {

		*_builder_i << (bbuf);

		if (_builder_i->reply_ready()) {
			_available_replies.push_back(_builder_i->get_reply());
			_builder_i = nullptr;

			_state = STATE_HEAD;
			return true;
		}

		_state = STATE_BODY;
		return false;
	};
}

//------------------------------------------------------------------------------
/**

*/
KjReplyBuilder::~KjReplyBuilder() {

	destroy_builders(_v_builder_i);
}

} //! builders

} //! cpp_redis