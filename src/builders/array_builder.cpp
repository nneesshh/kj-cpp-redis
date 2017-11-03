// The MIT License (MIT)
//
// Copyright (c) 2015-2017 Simon Ninon <simon.ninon@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "builders/array_builder.hpp"
#include "builders/builders_factory.hpp"

namespace cpp_redis {

namespace builders {

array_builder::array_builder() {
	// sub array = true
	create_builders(true, _v_builder_i);

	_row_state_cb[ROW_STATE_HEAD] = [this](bip_buf_t& bbuf)->bool {
		auto begin_sequence = bip_buf_get_contiguous_block(&bbuf);
		_row_builder_i = _v_builder_i[(*begin_sequence) - KJ_REPLY_BUILDER_SATRT_CHAR];
		bip_buf_decommit(&bbuf, 1);

		*_row_builder_i << bbuf;
		
		if (_row_builder_i->reply_ready()) {
			_reply << _row_builder_i->get_reply();
			_row_builder_i = nullptr;

			if (_reply.as_array().size() == _array_size)
				_reply_ready = true;

			// debug
			// 	fprintf(stderr, "rows=%d/%lld\n, break",
			// 		_reply.as_array().size(), _array_size);
			_row_state = ROW_STATE_HEAD;
			return true;
		}

		_row_state = ROW_STATE_BODY;
		return false;
	};

	_row_state_cb[ROW_STATE_BODY] = [this](bip_buf_t& bbuf)->bool {

		*_row_builder_i << bbuf;

		if (_row_builder_i->reply_ready()) {
			_reply << _row_builder_i->get_reply();
			_row_builder_i = nullptr;

			if (_reply.as_array().size() == _array_size)
				_reply_ready = true;

			// debug
			// 	fprintf(stderr, "rows=%d/%lld\n, break",
			// 		_reply.as_array().size(), _array_size);
			_row_state = ROW_STATE_HEAD;
			return true;
		}

		_row_state = ROW_STATE_BODY;
		return false;
	};
}

array_builder::~array_builder() {

	destroy_builders(_v_builder_i);
}

bool
array_builder::fetch_array_size(bip_buf_t& bbuf) {
	if (_head_builder.reply_ready())
		return true;

	_head_builder << bbuf;
	if (!_head_builder.reply_ready())
		return false;

	size_t sz = (size_t)_head_builder.get_integer();
	if (0 == sz) {
		_reply_ready = true;
	}

	_array_size = sz;
	return true;
}

builder_iface&
array_builder::operator<<(bip_buf_t& bbuf) {
	if (_reply_ready)
		return *this;

	if (!fetch_array_size(bbuf))
		return *this;

	while (!_reply_ready
		&& bip_buf_get_committed_size(&bbuf) > 0) {
		if (!build_row(bbuf))
			return *this;
	}

	return *this;
}

bool
array_builder::reply_ready() const {
	return _reply_ready;
}

CRedisReply&&
array_builder::get_reply() {
	CRedisReply r1 = _head_builder.get_reply();  // clear through &&
	_array_size = 0;
	_row_builder_i = nullptr;
	_reply_ready = false;
	return std::move(_reply);
}

} //! builders

} //! cpp_redis