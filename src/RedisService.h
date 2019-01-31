#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisService

(C) 2016 n.lee
*/
#include <string>

#include "base/redis_service_def.h"
#include "base/IRedisService.h"

#include "base/redis_extern.h"

//------------------------------------------------------------------------------
/**
@brief CRedisService
*/
class MY_REDIS_EXTERN CRedisService : public IRedisService {
public:
	CRedisService(void *servercore, redis_stub_param_t *param);
	virtual ~CRedisService() noexcept;

public:
	virtual void				OnUpdate() override {
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