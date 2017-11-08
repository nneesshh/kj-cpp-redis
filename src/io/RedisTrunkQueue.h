#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisTrunkQueue

(C) 2016 n.lee
*/
#include "base/CamelWorkQueue.h"
#include "base/IRedisService.h"

//------------------------------------------------------------------------------
/**
@brief CRedisTrunkQueue
*/
class CRedisTrunkQueue {
public:
	CRedisTrunkQueue();
	~CRedisTrunkQueue();

	void RunOnce() {
		_workQueue->RunOnce();
	}

	void Close() {
		_workQueue->Close();
	}

	void Add(IRedisService::reply_cb_t&&, CRedisReply&&);

private:
	CCamelWorkQueuePtr _workQueue;
};
using CRedisTrunkQueuePtr = std::shared_ptr<CRedisTrunkQueue>;

/*EOF*/