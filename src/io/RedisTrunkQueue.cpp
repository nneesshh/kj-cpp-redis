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
CRedisTrunkQueue::Add(IRedisService::reply_cb_t& cb, CRedisReply& reply) {
 	//
  	auto func = std::bind([](IRedisService::reply_cb_t& on_got_reply, CRedisReply& reply) {
  		on_got_reply(reply);
  	}, std::move(cb), std::move(reply));
 
 	//
 	_workQueue->Add(std::move(func));
}

/* -- EOF -- */