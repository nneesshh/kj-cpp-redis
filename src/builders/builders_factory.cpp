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
#include "builders/sub_array_builder.hpp"
#include "builders/builders_factory.hpp"
#include "builders/bulk_string_builder.hpp"
#include "builders/error_builder.hpp"
#include "builders/integer_builder.hpp"
#include "builders/simple_string_builder.hpp"

namespace cpp_redis {

namespace builders {

static builder_iface *
create_builder(char id) {
	switch (id) {
	case '+':
		return new simple_string_builder();
	case '-':
		return new error_builder();
	case ':':
		return new integer_builder();
	case '$':
		return new bulk_string_builder();

	default:
		return nullptr;
	}
	
}

static builder_iface *
create_array_builder(char id) {
	switch (id) {
	case '*':
		return new array_builder();

	default:
		return nullptr;
	}

}

static builder_iface *
create_sub_array_builder(char id) {
	switch (id) {
	case '*':
		return new sub_array_builder();

	default:
		return nullptr;
	}

}

void
create_builders(bool sub_array, std::vector<builder_iface *>& v_out) {
	char ch;
	for (ch = KJ_REPLY_BUILDER_SATRT_CHAR; ch <= KJ_REPLY_BUILDER_OVER_CHAR; ++ch) {

		builder_iface *builder_i = nullptr;
		if (sub_array) {
			builder_i = create_sub_array_builder(ch);
		}
		else {
			builder_i = create_array_builder(ch);
		}

		if (nullptr == builder_i) {
			builder_i = create_builder(ch);
		}

		v_out.push_back(builder_i);
	}
}

void
destroy_builders(std::vector<builder_iface *>& v_out) {
	char ch;
	for (auto& it : v_out) {
		delete(it);
	}
}

builder_iface *
create_dynamic_builder(char id) {
	switch (id) {
	case '+':
		return new simple_string_builder();
	case '-':
		return new error_builder();
	case ':':
		return new integer_builder();
	case '$':
		return new bulk_string_builder();
	case '*':
		return new sub_array_builder();

	default:
		return nullptr;
	}

}

} //! builders

} //! cpp_redis