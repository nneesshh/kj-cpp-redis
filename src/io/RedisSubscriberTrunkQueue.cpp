//------------------------------------------------------------------------------
//  RedisSubscriberTrunkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisSubscriberTrunkQueue.hpp"

#include "../RedisRootContextDef.hpp"
#include "../RedisSubscriber.h"

//------------------------------------------------------------------------------
/**

*/
CRedisSubscriberTrunkQueue::CRedisSubscriberTrunkQueue(CRedisSubscriber *pRedisHandle)
	: _refRedisHandle(pRedisHandle)
	, _callbacks(std::make_shared<CCamelReaderWriterQueue>()) {

}

//------------------------------------------------------------------------------
/**

*/
CRedisSubscriberTrunkQueue::~CRedisSubscriberTrunkQueue() {

}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriberTrunkQueue::Add(std::function<void()>&& workCb) {

	_callbacks->Add(std::move(workCb));

	// write opcode to pipe
	++_refRedisHandle->_trunkOpCodeSend;

	kj::AsyncIoStream& pipeEndPoint = _refRedisHandle->_refPipeWorker->endpointContext->GetEndpoint();
	pipeEndPoint.write((const void *)&_refRedisHandle->_trunkOpCodeSend, 1);
	redis_get_servercore()->PipeNotify(pipeEndPoint, _refRedisHandle->_trunkOpCodeSend);
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisSubscriberTrunkQueue::Add(redis_reply_cb_t&& cb, CRedisReply&& r) {

	auto workCb = std::bind([](redis_reply_cb_t& on_got_reply, CRedisReply& reply) {
  		on_got_reply(std::move(reply));
  	}, std::move(cb), std::move(r));

 	Add(std::move(workCb));
}

/* -- EOF -- */