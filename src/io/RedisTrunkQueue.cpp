//------------------------------------------------------------------------------
//  RedisTrunkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisTrunkQueue.h"

//------------------------------------------------------------------------------
/**

*/
CRedisTrunkQueue::CRedisTrunkQueue()
	: _workQueue(std::make_shared<CCamelConcurrentWorkQueue>()) {

}

//------------------------------------------------------------------------------
/**

*/
CRedisTrunkQueue::~CRedisTrunkQueue() {

}

//------------------------------------------------------------------------------
/**

*/
void
CRedisTrunkQueue::Add(redis_reply_cb_t&& cb, CRedisReply&& r) {

	auto workCb = std::bind([](redis_reply_cb_t& on_got_reply, CRedisReply& reply) {
  		on_got_reply(std::move(reply));
  	}, std::move(cb), std::move(r));

 	Add(std::move(workCb));
}

/* -- EOF -- */