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

#pragma once

#include <functional>

#include "builders/builder_iface.hpp"
#include "builders/integer_builder.hpp"

namespace cpp_redis {

namespace builders {

class sub_array_builder : public builder_iface {
public:
	//! ctor & dtor
	sub_array_builder();
	~sub_array_builder();

	//! copy ctor & assignment operator
	sub_array_builder(const sub_array_builder&) = delete;
	sub_array_builder& operator=(const sub_array_builder&) = delete;

	enum ROW_STATE {
		ROW_STATE_HEAD = 0,
		ROW_STATE_BODY = 1,
	};

public:
	//! builder_iface impl
	builder_iface& operator<<(bip_buf_t& bbuf);
	bool reply_ready() const;
	CRedisReply&& get_reply();

private:
	bool fetch_array_size(bip_buf_t& bbuf);
	bool build_row(bip_buf_t& bbuf);

private:
	std::function<bool(bip_buf_t&)> _row_state_cb[ROW_STATE_BODY + 1];

	integer_builder _head_builder;
	uint64_t _array_size = 0;

	ROW_STATE _row_state = ROW_STATE_HEAD;
	builder_iface *_row_builder_i = nullptr;

	bool _reply_ready = false;;
	CRedisReply _reply;
};

} //! builders

} //! cpp_redis