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

#include <cctype>

#include "builders/integer_builder.hpp"

namespace cpp_redis {

namespace builders {

integer_builder::integer_builder() {}

builder_iface&
integer_builder::operator<<(bip_buf_t& bbuf) {
	if (_reply_ready)
		return *this;

	auto buf_size = bip_buf_get_committed_size(&bbuf);
	if (buf_size < 2)
		return *this;

	// wait for end_sequence
	auto end_sequence = bip_buf_find_str(&bbuf, "\r\n", 2);
	if (nullptr == end_sequence)
		return *this;

	auto begin_sequence = bip_buf_get_contiguous_block(&bbuf);
	char *p;
	for (p = begin_sequence; p < end_sequence; ++p) {
		//! check for negative numbers
		if (_negative_multiplicator == 1 && (*p) == '-') {
			_negative_multiplicator = -1;
			continue;
		}
		else if (!std::isdigit(*p)) {
			throw CRedisError("Invalid character for integer redis reply");
		}

		_nbr *= 10;
		_nbr += (*p) - '0';
	}

	bip_buf_decommit(&bbuf, end_sequence + 2 - begin_sequence);
	_reply.set(_negative_multiplicator * _nbr);
	_reply_ready = true;

	return *this;
}

bool
integer_builder::reply_ready() const {
	return _reply_ready;
}

CRedisReply&&
integer_builder::get_reply() {
	_nbr = 0;
	_negative_multiplicator = 1;
	_reply_ready = false;
	return std::move(_reply);
}

int64_t
integer_builder::get_integer() const {
	return _negative_multiplicator * _nbr;
}

} //! builders

} //! cpp_redis