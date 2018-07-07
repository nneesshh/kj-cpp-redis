#pragma once
//------------------------------------------------------------------------------
/**
@class CCamelReaderWriterQueue

(C) 2016 n.lee
*/
#include <memory>
#include <functional>

#include "concurrent/readerwriterqueue.h"

//------------------------------------------------------------------------------
/**
@brief CCamelReaderWriterQueue
*/
class CCamelReaderWriterQueue {
public:
	CCamelReaderWriterQueue();
	~CCamelReaderWriterQueue();

	using CallbackEntry = std::function<void()>;

	void RunOnce();
	void Close();

	bool Add(std::function<void()>&& workCb);

private:
	bool _close = false;

	moodycamel::ReaderWriterQueue<CallbackEntry> _callbacks;
};
using CCamelReaderWriterQueuePtr = std::shared_ptr<CCamelReaderWriterQueue>;

/*EOF*/