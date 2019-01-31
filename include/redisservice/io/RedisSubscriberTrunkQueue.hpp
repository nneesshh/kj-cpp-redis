#pragma once
//------------------------------------------------------------------------------
/**
	@class CRedisSubscriberTrunkQueue

	(C) 2016 n.lee
*/
#include "base/CamelReaderWriterQueue.h"
#include "base/IRedisService.h"

#include "KjRedisSubscriberWorkQueue.hpp"

class CRedisSubscriber;

//------------------------------------------------------------------------------
/**
@brief CRedisSubscriberTrunkQueue
*/
class CRedisSubscriberTrunkQueue {
public:
	CRedisSubscriberTrunkQueue(CRedisSubscriber *pRedisHandle);
	~CRedisSubscriberTrunkQueue();

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
	CRedisSubscriber *_refRedisHandle;

};
using CRedisSubscriberTrunkQueuePtr = std::shared_ptr<CRedisSubscriberTrunkQueue>;

/*EOF*/