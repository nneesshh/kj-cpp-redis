//------------------------------------------------------------------------------
//  CamelWorkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "CamelWorkQueue.h"

//------------------------------------------------------------------------------
/**

*/
CCamelConcurrentWorkQueue::CCamelConcurrentWorkQueue()
	: _callbacks(256) {

}

//------------------------------------------------------------------------------
/**

*/
CCamelConcurrentWorkQueue::~CCamelConcurrentWorkQueue() {

}

//------------------------------------------------------------------------------
/**

*/
void
CCamelConcurrentWorkQueue::RunOnce() {
	// work queue
	int nCount = 0;
	CallbackEntry workCb;
	while (!_close && _callbacks.try_dequeue(workCb)) {
		workCb();

		//
		++nCount;
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CCamelConcurrentWorkQueue::Close() {
	//
	// Set done flag and notify.
	//
	_close = true;
}

//------------------------------------------------------------------------------
/**

*/
bool
CCamelConcurrentWorkQueue::Add(std::function<void()>&& workCb) {
	if (_close) {
		// error
		fprintf(stderr, "[CCamelWorkQueue::Add()] can't enqueue, callback is dropped!!!");
		return false;
	}

	//
	// Add work item.
	//
	if (!_callbacks.enqueue(std::move(workCb))) {
		// error
		fprintf(stderr, "[CCamelWorkQueue::Add()] enqueue failed, callback is dropped!!!");
		return false;
	}
	return true;
}

/** -- EOF -- **/