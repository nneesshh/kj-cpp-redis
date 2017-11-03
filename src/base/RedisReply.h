#pragma once

//------------------------------------------------------------------------------
/**
@class CRedisReply

(C) 2016 n.lee
*/
#include <iostream>
#include <string>
#include <vector>

#include <stdint.h>

//------------------------------------------------------------------------------
/**
@brief CRedisReply
*/
class CRedisReply {
public:
	//! type of reply
#define __CPP_REDIS_REPLY_ERR 0
#define __CPP_REDIS_REPLY_BULK 1
#define __CPP_REDIS_REPLY_SIMPLE 2
#define __CPP_REDIS_REPLY_NULL 3
#define __CPP_REDIS_REPLY_INT 4
#define __CPP_REDIS_REPLY_ARRAY 5

	enum class type {
		error = __CPP_REDIS_REPLY_ERR,
		bulk_string = __CPP_REDIS_REPLY_BULK,
		simple_string = __CPP_REDIS_REPLY_SIMPLE,
		null = __CPP_REDIS_REPLY_NULL,
		integer = __CPP_REDIS_REPLY_INT,
		array = __CPP_REDIS_REPLY_ARRAY
	};

	enum class string_type {
		error = __CPP_REDIS_REPLY_ERR,
		bulk_string = __CPP_REDIS_REPLY_BULK,
		simple_string = __CPP_REDIS_REPLY_SIMPLE
	};

public:
	//! ctors
	CRedisReply();
	CRedisReply(const std::string& value, string_type reply_type);
	CRedisReply(int64_t value);
	CRedisReply(const std::vector<CRedisReply>& rows);
	CRedisReply(CRedisReply&&);

	//! dtors & copy ctor & assignment operator
	~CRedisReply() = default;
	CRedisReply(const CRedisReply&) = default;
	CRedisReply& operator=(const CRedisReply&) = default;
	CRedisReply& operator=(CRedisReply&&);

public:
	//! type info getters
	bool is_array() const;
	bool is_string() const;
	bool is_simple_string() const;
	bool is_bulk_string() const;
	bool is_error() const;
	bool is_integer() const;
	bool is_null() const;

	//! convenience function for error handling
	bool ok() const;
	bool ko() const;
	const std::string& error() const;

	//! convenience implicit conversion, same as !is_null()
	operator bool() const;

	//! Value getters
	const std::vector<CRedisReply>& as_array() const;
	const std::string& as_string() const;
	int64_t as_integer() const;

	//! Value setters
	void set();
	void set(std::string& value, string_type reply_type);
	void set(int64_t value);
	void set(std::vector<CRedisReply>& rows);
	CRedisReply& operator<<(CRedisReply&& reply);

	//! type getter
	type get_type() const;

private:
	type _type = type::null;
	std::vector<CRedisReply> _rows;
	std::string _strval;
	int64_t _intval = 0;
};

//! support for output
std::ostream& operator<<(std::ostream& os, const CRedisReply& reply);

/*EOF*/