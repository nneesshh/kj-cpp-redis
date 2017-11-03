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

builder_iface&
simple_string_builder::operator<<(bip_buf_t& bbuf) {
	if (_reply_ready)
		return *this;

	auto end_sequence = bip_buf_find_str(&bbuf, "\r\n", 2);
	if (nullptr == end_sequence)
		return *this;

	auto begin_sequence = bip_buf_get_contiguous_block(&bbuf);
	_str = std::string(begin_sequence, end_sequence);
	bip_buf_decommit(&bbuf, end_sequence + 2 - begin_sequence);
	_reply.set(_str, CRedisReply::string_type::simple_string);
	_reply_ready = true;

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

std::string&
simple_string_builder::get_simple_string() {
	return _str;
}

} //! builders

} //! cpp_redis