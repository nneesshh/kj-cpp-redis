#pragma once
//------------------------------------------------------------------------------
/**
@class CCamelWorkQueue

(C) 2016 n.lee
*/
#include <memory>
#include <functional>

#include "concurrent/readerwriterqueue.h"

//------------------------------------------------------------------------------
/**
@brief CCamelWorkQueue
*/
class CCamelWorkQueue {
public:
	CCamelWorkQueue();
	~CCamelWorkQueue();

	using CallbackEntry = std::function<void()>;

	void RunOnce();
	void Close();

	bool Add(std::function<void()>&& workCb);

private:
	bool _close = false;

	moodycamel::ReaderWriterQueue<CallbackEntry> _callbacks;
};
using CCamelWorkQueuePtr = std::shared_ptr<CCamelWorkQueue>;

/*EOF*/