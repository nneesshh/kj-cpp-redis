#pragma once
//------------------------------------------------------------------------------
/**
@class CKjRedisWorkQueue

(C) 2016 n.lee
*/
#include <deque>
#include <vector>
#include <mutex>
#include <thread>

#include "base/concurrent/readerwriterqueue.h"
#include "base/IRedisService.h"

#include "io/KjSimpleIoContext.h"
#include "io/KjSimpleThreadIoContext.h"
#include "io/KjRedisConnection.hpp"

struct redis_thread_worker_t {
	kj::Own<KjSimpleThreadIoContext>	_tioContext;
	kj::Own<kj::TaskSet>				_tasks;
	kj::Own<KjRedisConnection>			_conn;
};

//------------------------------------------------------------------------------
/**
@brief CKjRedisWorkQueue
*/
class CKjRedisWorkQueue {
public:
	CKjRedisWorkQueue(redis_stub_param_t& param);
	~CKjRedisWorkQueue();

	using CallbackEntry = IRedisService::cmd_pipepline_t;

	bool						Add(IRedisService::cmd_pipepline_t&& cmd);

	bool						IsDone() {
		return _done;
	}

	moodycamel::ReaderWriterQueue<CallbackEntry>& Callbacks() {
		return _callbacks;
	}

	void						Finish();

private:
	void						Start();
	void						Run(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope);

private:
	redis_stub_param_t& _refParam;

	bool _done = false;
	moodycamel::ReaderWriterQueue<CallbackEntry> _callbacks;

	// threads
	kj::Own<KjSimpleIoContext> _pipeContext;
	kj::AsyncIoProvider::PipeThread _pipeThread;

public:
	char _opCodeSend = 0;
	char _opCodeRecv = 0;

	CallbackEntry _opCmd;

};
using CKjRedisWorkQueuePtr = std::shared_ptr<CKjRedisWorkQueue>;

/*EOF*/