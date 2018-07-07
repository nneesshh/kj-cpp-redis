//------------------------------------------------------------------------------
//  RedisReply.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisError.h"
#include "RedisReply.h"

CRedisReply::CRedisReply() {}

CRedisReply::CRedisReply(const std::string& value, string_type reply_type)
	: _type(static_cast<type>(reply_type))
	, _strval(value)
	, _intval(0) {}

CRedisReply::CRedisReply(int64_t value)
	: _type(type::integer)
	, _intval(value) {}

CRedisReply::CRedisReply(const std::vector<CRedisReply>& rows)
	: _type(type::array)
	, _rows(rows)
	, _intval(0) {}

CRedisReply::CRedisReply(CRedisReply&& rhs)
	: _type(rhs._type)
	, _rows(std::move(rhs._rows))
	, _strval(std::move(rhs._strval))
	, _intval(rhs._intval) {}

CRedisReply&
CRedisReply::operator=(CRedisReply&& rhs) {
	_type = rhs._type;
	_rows = std::move(rhs._rows);
	_strval = std::move(rhs._strval);
	_intval = rhs._intval;

	rhs._type = type::null;
	rhs._intval = 0;
	return *this;
}

bool
CRedisReply::ok() const {
	return !is_error();
}

bool
CRedisReply::ko() const {
	return !ok();
}

std::string&
CRedisReply::error_desc() {
	if (!is_error())
		throw CRedisError("Reply is not an error");

	return as_string();
}

CRedisReply::operator bool() const {
	return !is_error() && !is_null();
}

void
CRedisReply::set() {
	_type = type::null;
}

void
CRedisReply::set(const std::string& value, string_type reply_type) {
	_type = static_cast<type>(reply_type);
	_strval = std::move(value);
}

void
CRedisReply::set(int64_t value) {
	_type = type::integer;
	_intval = value;
}

void
CRedisReply::set(std::vector<CRedisReply>& rows) {
	_type = type::array;
	_rows = std::move(rows);
}

CRedisReply&
CRedisReply::operator<<(CRedisReply&& reply) {
	_type = type::array;
	_rows.emplace_back(std::move(reply));

	return *this;
}

bool
CRedisReply::is_array() const {
	return _type == type::array;
}

bool
CRedisReply::is_string() const {
	return is_simple_string() || is_bulk_string() || is_error();
}

bool
CRedisReply::is_simple_string() const {
	return _type == type::simple_string;
}

bool
CRedisReply::is_bulk_string() const {
	return _type == type::bulk_string;
}

bool
CRedisReply::is_error() const {
	return _type == type::error;
}

bool
CRedisReply::is_integer() const {
	return _type == type::integer;
}

bool
CRedisReply::is_null() const {
	return _type == type::null;
}

std::vector<CRedisReply>&
CRedisReply::as_array() {
	if (!is_array())
		throw CRedisError("Reply is not an array");

	return _rows;
}

std::string&
CRedisReply::as_string() {
	if (!is_string())
		throw CRedisError("Reply is not a string");

	return _strval;
}

int64_t
CRedisReply::as_integer() const {
	if (!is_integer())
		throw CRedisError("Reply is not an integer");

	return _intval;
}

CRedisReply::type
CRedisReply::get_type() const {
	return _type;
}

std::ostream&
operator<<(std::ostream& os, CRedisReply&& reply) {
	switch (reply.get_type()) {
	case CRedisReply::type::error:
		os << reply.error_desc();
		break;
	case CRedisReply::type::bulk_string:
		os << reply.as_string();
		break;
	case CRedisReply::type::simple_string:
		os << reply.as_string();
		break;
	case CRedisReply::type::null:
		os << std::string("(nil)");
		break;
	case CRedisReply::type::integer:
		os << reply.as_integer();
		break;
	case CRedisReply::type::array:
		for (const auto& item : reply.as_array())
			os << item;
		break;
	}

	return os;
}

/** -- EOF -- **/