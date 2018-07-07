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

#include "builders/bulk_string_builder.hpp"

namespace cpp_redis {

namespace builders {

bulk_string_builder::bulk_string_builder() {}

void
bulk_string_builder::build_reply() {
	if (_is_null)
		_reply.set();
	else
		_reply.set(_str, CRedisReply::string_type::bulk_string);

	_reply_ready = true;
}

bool
bulk_string_builder::fetch_size(bip_buf_t& bbuf) {
	if (_head_builder.reply_ready())
		return true;

	_head_builder << bbuf;
	if (!_head_builder.reply_ready())
		return false;

	_str_size = (int)_head_builder.get_integer();
	if (_str_size == -1) {
		// "$-1\r\n" means null
		_is_null = true;
		build_reply();
	}

	// pre-alloc memory
	_str.reserve(_str_size);
	return true;
}

void
bulk_string_builder::fetch_str(bip_buf_t& bbuf) {
	if (_reply_ready)
		return;

	auto buf_size = bip_buf_get_committed_size(&bbuf);
	auto begin_sequence = bip_buf_get_contiguous_block(&bbuf);
	auto want_size = _str_size + 2 - _str.length();
	size_t consume_size = 0;

	// consume
	if (buf_size < want_size) {
		consume_size = buf_size;
		_str.append(begin_sequence, consume_size);
	}
	else {
		consume_size = want_size;
		_str.append(begin_sequence, consume_size);

		// validate end sequence
		auto end_sequence = _str.c_str() + _str_size;
		if (end_sequence[0] != '\r' || end_sequence[1] != '\n') {
			throw CRedisError("Wrong ending sequence");
		}

		// strip end sequence
		_str.resize(_str_size - 2);
		build_reply();
	}

	bip_buf_decommit(&bbuf, consume_size);
}

builder_iface&
bulk_string_builder::operator<<(bip_buf_t& bbuf) {
	if (_reply_ready)
		return *this;

	//! if we don't have the size, try to get it with the current buffer
	if (!fetch_size(bbuf) || _reply_ready)
		return *this;

	fetch_str(bbuf);

	return *this;
}

bool
bulk_string_builder::reply_ready() const {
	return _reply_ready;
}

CRedisReply&& 
bulk_string_builder::get_reply() {
	CRedisReply r1 = _head_builder.get_reply(); // clear through &&
	_str_size = 0;
	_str.clear();
	_is_null = false;
	_reply_ready = false;
	return std::move(_reply);
}

bool
bulk_string_builder::is_null() const {
	return _is_null;
}

} //! builders

} //! cpp_redis