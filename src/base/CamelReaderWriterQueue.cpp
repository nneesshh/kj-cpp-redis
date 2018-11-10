//------------------------------------------------------------------------------
//  CamelReaderWriterQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "CamelReaderWriterQueue.h"

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif

static void
util_sleep(unsigned int milliseconds) {
#if defined(WIN32) || defined(_WIN32)
	Sleep(milliseconds);
#else
	int seconds = milliseconds / 1000;
	int useconds = (milliseconds % 1000) * 1000;

	sleep(seconds);
	usleep(useconds);
#endif
}

//------------------------------------------------------------------------------
/**

*/
CCamelReaderWriterQueue::CCamelReaderWriterQueue()
	: _callbacks(256) {

}

//------------------------------------------------------------------------------
/**

*/
CCamelReaderWriterQueue::~CCamelReaderWriterQueue() {

}

//------------------------------------------------------------------------------
/**

*/
void
CCamelReaderWriterQueue::RunOnce() {
	
	int ncount = 0;

	// work queue
	CallbackEntry workCb;
	while (!_close 
		&& _callbacks.try_dequeue(workCb)) {

		workCb();

		//
		++ncount;
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CCamelReaderWriterQueue::Close() {
	//
	// Set done flag and notify.
	//
	_close = true;
}

//------------------------------------------------------------------------------
/**

*/
bool
CCamelReaderWriterQueue::Add(std::function<void()>&& workCb) {
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