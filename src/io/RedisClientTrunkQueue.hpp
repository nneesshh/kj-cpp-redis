#pragma once
//------------------------------------------------------------------------------
/**
	@class CRedisClientTrunkQueue

	(C) 2016 n.lee
*/
#include "base/CamelReaderWriterQueue.h"
#include "base/IRedisService.h"

#include "KjRedisClientWorkQueue.hpp"

class CRedisClient;

//------------------------------------------------------------------------------
/**
@brief CRedisClientTrunkQueue
*/
class CRedisClientTrunkQueue {
public:
	CRedisClientTrunkQueue(CRedisClient *pRedisHandle);
	~CRedisClientTrunkQueue();

	void RunOnce() {
		_callbacks->RunOnce();
	}

	void Close() {
		_callbacks->Close();
	}

	void Add(std::function<void()>&& workCb);

	void Add(redis_reply_cb_t&&, CRedisReply&&);

private:
	CCamelReaderWriterQueuePtr _callbacks;

public:
	CRedisClient *_refRedisHandle;

};
using CRedisClientTrunkQueuePtr = std::shared_ptr<CRedisClientTrunkQueue>;

/*EOF*/