//------------------------------------------------------------------------------
//  RedisClientTrunkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisClientTrunkQueue.hpp"

#include "../RedisRootContextDef.hpp"
#include "../RedisClient.h"

//------------------------------------------------------------------------------
/**

*/
CRedisClientTrunkQueue::CRedisClientTrunkQueue(CRedisClient *pRedisHandle)
	: _refRedisHandle(pRedisHandle)
	, _callbacks(std::make_shared<CCamelReaderWriterQueue>()) {

}

//------------------------------------------------------------------------------
/**

*/
CRedisClientTrunkQueue::~CRedisClientTrunkQueue() {

}

//------------------------------------------------------------------------------
/**

*/
void
CRedisClientTrunkQueue::Add(std::function<void()>&& workCb) {

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
CRedisClientTrunkQueue::Add(redis_reply_cb_t&& cb, CRedisReply&& r) {

	auto workCb = std::bind([](redis_reply_cb_t& on_got_reply, CRedisReply& reply) {
  		on_got_reply(std::move(reply));
  	}, std::move(cb), std::move(r));

 	Add(std::move(workCb));
}

/* -- EOF -- */