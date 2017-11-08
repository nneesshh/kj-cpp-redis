//------------------------------------------------------------------------------
//  RedisTrunkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisTrunkQueue.h"

//------------------------------------------------------------------------------
/**

*/
CRedisTrunkQueue::CRedisTrunkQueue()
	: _workQueue(std::make_shared<CCamelWorkQueue>()) {

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
CRedisTrunkQueue::Add(IRedisService::reply_cb_t&& cb, CRedisReply&& r) {
 	//
	std::function<void()> func = std::bind([](IRedisService::reply_cb_t& on_got_reply, CRedisReply& reply) {
  		on_got_reply(std::move(reply));
  	}, std::move(cb), std::move(r));
 
 	//
 	_workQueue->Add(std::move(func));
}

/* -- EOF -- */