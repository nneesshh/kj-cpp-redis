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
	redis_service_entry_t() {
		_nId = 0;
		_redisservice = nullptr;
	}

	~redis_service_entry_t() {
		_redisservice = nullptr;
	}

	int _nId;
	redis_stub_param_t _param;
	void *_redisservice;

	std::string _sModuleName;
	std::string _cacheDirtyEntry;
	std::string _listDirtyEntry;
	int _dumpInterval;
};

/*EOF*/