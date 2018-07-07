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

#include "builders/simple_string_builder.hpp"

namespace cpp_redis {

namespace builders {

simple_string_builder::simple_string_builder() {}

void
simple_string_builder::build_reply() {
	_reply.set(_str, CRedisReply::string_type::simple_string);

	_reply_ready = true;
}

builder_iface&
simple_string_builder::operator<<(bip_buf_t& bbuf) {
	if (_reply_ready)
		return *this;

	auto buf_size = bip_buf_get_committed_size(&bbuf);
	if (buf_size < 2)
		return *this;

	auto begin_sequence = bip_buf_get_contiguous_block(&bbuf);
	auto end_sequence = bip_buf_find_str(&bbuf, "\r\n", 2);
	size_t consume_size = 0;

	// consume
	if (nullptr == end_sequence) {
		bool end_with_r = ('\r' == *(char *)(begin_sequence + buf_size - 1));
		// leave one char if it is '\r'
		consume_size = end_with_r ? buf_size - 1 : buf_size;
		_str.append(begin_sequence, consume_size);
	}
	else {
		consume_size = end_sequence + 2 - begin_sequence;
		_str.append(begin_sequence, consume_size - 2);
		build_reply();
	}

	bip_buf_decommit(&bbuf, consume_size);
	return *this;
}

bool
simple_string_builder::reply_ready() const {
	return _reply_ready;
}

CRedisReply&&
simple_string_builder::get_reply() {
	_str.clear();
	_reply_ready = false;
	return std::move(_reply);
}

} //! builders

} //! cpp_redis