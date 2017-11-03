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

#include "builders/error_builder.hpp"

namespace cpp_redis {

namespace builders {

builder_iface&
error_builder::operator<<(bip_buf_t& bbuf) {
  _msg_builder << bbuf;

  if (_msg_builder.reply_ready())
    _reply.set(_msg_builder.get_simple_string(), CRedisReply::string_type::error);

  return *this;
}

bool
error_builder::reply_ready() const {
  return _msg_builder.reply_ready();
}

CRedisReply&&
error_builder::get_reply() {
	CRedisReply r1 = _msg_builder.get_reply(); // clear through &&
	return std::move(_reply);
}

std::string&
error_builder::get_error() {
  return _msg_builder.get_simple_string();
}

} //! builders

} //! cpp_redis
