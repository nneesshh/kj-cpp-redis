#pragma once

//------------------------------------------------------------------------------
/**
@class CRedisReply

(C) 2016 n.lee
*/
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <stdint.h>

#include "redis_extern.h"

class CRedisReply;
using redis_reply_cb_t = std::function<void(CRedisReply&&)>;

using dispose_cb_t = std::function<void()>;

struct redis_cmd_pipepline_t {
	/* pipeline state */
	enum PIPELINE_STATE {
		QUEUEING = 1,
		SENDING = 2,
		COMMITTING = 3,
		PROCESSING = 4,
		PROCESS_OVER = 5,
	};

	int _sn;
	std::string _commands;
	int _built_num;
	int _processed_num;
	redis_reply_cb_t _reply_cb;
	dispose_cb_t _dispose_cb;
	PIPELINE_STATE _state;
};

//------------------------------------------------------------------------------
/**
@brief CRedisReply
*/
class MY_REDIS_EXTERN CRedisReply {
public:
	enum REPLY_TYPE {
		REDIS_REPLY_TYPE_ERR = 0,
		REDIS_REPLY_TYPE_BULK = 1,
		REDIS_REPLY_TYPE_SIMPLE = 2,
		REDIS_REPLY_TYPE_NULL = 3,
		REDIS_REPLY_TYPE_INT = 4,
		REDIS_REPLY_TYPE_ARRAY = 5,
	};

	enum class type {
		error = REDIS_REPLY_TYPE_ERR,
		bulk_string = REDIS_REPLY_TYPE_BULK,
		simple_string = REDIS_REPLY_TYPE_SIMPLE,
		null = REDIS_REPLY_TYPE_NULL,
		integer = REDIS_REPLY_TYPE_INT,
		array = REDIS_REPLY_TYPE_ARRAY
	};

	enum class string_type {
		error = REDIS_REPLY_TYPE_ERR,
		bulk_string = REDIS_REPLY_TYPE_BULK,
		simple_string = REDIS_REPLY_TYPE_SIMPLE
	};

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
	std::string& error_desc();

	//! convenience implicit conversion, same as !is_null()
	operator bool() const;

	//! Value getters
	std::vector<CRedisReply>& as_array();
	std::string& as_string();
	const std::string& as_safe_string();
	int64_t as_integer() const;

	//! Value setters
	void set();
	void set(std::string&& value, string_type reply_type);
	void set(int64_t value);
	void set(std::vector<CRedisReply>&& rows);

	//! type getter
	type get_type() const;

private:
	type _type = type::null;
	std::vector<CRedisReply> _rows;
	std::string _strval;
	int64_t _intval = 0;
};

//! support for output
std::ostream& operator<<(std::ostream& os, CRedisReply&& reply);

/*EOF*/