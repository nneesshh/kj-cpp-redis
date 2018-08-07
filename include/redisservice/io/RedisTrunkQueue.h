#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisTrunkQueue

(C) 2016 n.lee
*/
#include "base/CamelReaderWriterQueue.h"
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

	void Add(std::function<void()>&& workCb) {
		_workQueue->Add(std::move(workCb));
	}

	void Add(redis_reply_cb_t&&, CRedisReply&&);

private:
	CCamelReaderWriterQueuePtr _workQueue;
};
using CRedisTrunkQueuePtr = std::shared_ptr<CRedisTrunkQueue>;

/*EOF*/