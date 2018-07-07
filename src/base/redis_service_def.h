#pragma once

#include <string>
#include <map>

struct redis_stub_param_t {
	std::string _ip;
	unsigned short _port;

	std::string _sPassword;
	std::map<std::string, std::string> _mapScript;
};

struct redis_service_entry_t {
	int _nId;
	std::string _sModuleName;
	redis_stub_param_t _param;
	void *_redisservice;

	std::string _cacheDirtyEntry;
	std::string _listDirtyEntry;
	int _dumpInterval;

	std::string	_queryCacheKey;
	std::string	_updateCacheKey;
};

/*EOF*/