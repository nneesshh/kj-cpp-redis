#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisService

(C) 2016 n.lee
*/
#include <string>

#include "base/redis_service_def.h"
#include "base/IRedisService.h"

//------------------------------------------------------------------------------
/**
@brief CRedisService
*/
class CRedisService : public IRedisService {
public:
	CRedisService(redis_stub_param_t *param);
	virtual ~CRedisService();

public:
	virtual void				RunOnce() override {
		_redisClient->RunOnce();
		_redisSubscriber->RunOnce();
	}

	virtual IRedisClient&		Client() override {
		return *_redisClient;
	}

	virtual IRedisSubscriber&	Subscriber() override {
		return *_redisSubscriber;
	}

	virtual int					ParseDumpedData(const std::string& sDump, std::function<int(rdb_object_t *)>&& cb) override;

	virtual void				Shutdown() override;

private:
	redis_stub_param_t _param;
	bool _bShutdown = false;

	IRedisClient *_redisClient;
	IRedisSubscriber *_redisSubscriber;

	rdb_parser_t *_rp = nullptr;

};

/*EOF*/