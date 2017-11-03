#pragma once

//------------------------------------------------------------------------------
/**
@class CRedisError

(C) 2016 n.lee
*/
#include <stdexcept>
#include <string>

//------------------------------------------------------------------------------
/**
@brief CRedisError
*/
class CRedisError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
	using std::runtime_error::what;

	explicit CRedisError(const std::string& sMessage)
		: std::runtime_error(sMessage.c_str()) { // construct from message string
	}

	explicit CRedisError(const char * sMessage)
		: std::runtime_error(sMessage) { // construct from message string
	}
};

/*EOF*/
