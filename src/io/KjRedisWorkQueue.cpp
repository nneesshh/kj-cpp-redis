//------------------------------------------------------------------------------
//  KjRedisWorkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjRedisWorkQueue.h"

static kj::Promise<void>
check_quit_loop(CKjRedisWorkQueue& q, redis_thread_worker_t& worker, kj::PromiseFulfiller<void> *fulfiller) {
	if (!q.IsDone()) {
		// wait 500 ms
		return worker._tioContext->GetTimer().afterDelay(500 * kj::MICROSECONDS, "delay and check quit loop")
			.then([&q, &worker, fulfiller]() {
			// loop
			return check_quit_loop(q, worker, fulfiller);
		});
	}

	//
	fulfiller->fulfill();
	return kj::READY_NOW;
}

static kj::Promise<void>
read_pipe_loop(CKjRedisWorkQueue& q, redis_thread_worker_t& worker, kj::AsyncIoStream& stream) {

	if (!q.IsDone()) {
		return stream.tryRead(&q._opCodeRecv, 1, 1)
			.then([&q, &worker, &stream](size_t amount) {
			//
			// Get next work item.
			//
			int nCount = 0;
			int nCmdSn = 0;
			CKjRedisWorkQueue::CallbackEntry entry;
			while (q.Callbacks().try_dequeue(entry)) {
				auto& cmd = std::get<0>(entry);
				worker._conn->Send(cmd);
				++nCount;
				nCmdSn = cmd._sn;
			}

			if (nCount > 0) {
				worker._conn->Commit();
			}
			return read_pipe_loop(q, worker, stream);
		});
	}
	return kj::READY_NOW;
}

//------------------------------------------------------------------------------
/**

*/
CKjRedisWorkQueue::CKjRedisWorkQueue(redis_stub_param_t& param)
	: _refParam(param)
	, _callbacks(256) {
	//
	_pipeContext = kj::refcounted<KjSimpleIoContext>();

	//
	Start();
}

//------------------------------------------------------------------------------
/**

*/
CKjRedisWorkQueue::~CKjRedisWorkQueue() {
	_pipeContext = nullptr;

	_pipeThread.pipe = nullptr;
	_pipeThread.thread = nullptr;
}

//------------------------------------------------------------------------------
/**

*/
bool
CKjRedisWorkQueue::Add(IRedisService::cmd_t& cmd) {

	if (_done) {
		// error
		fprintf(stderr, "[CKjRedisWorkQueue::Add()] can't enqueue, callback is dropped!!!");
		return false;
	}

	//
	// Add work item.
	//
	CallbackEntry entry = std::make_tuple(std::move(cmd));
	if (!_callbacks.enqueue(std::move(entry))) {
		// error
		fprintf(stderr, "[CKjRedisWorkQueue::Add()] enqueue failed, callback is dropped!!!");
		return false;
	}

	// write opcode to pipe
	++_opCodeSend;
	_pipeThread.pipe->write((const void *)&_opCodeSend, 1);
	return true;
}

//------------------------------------------------------------------------------
/**

*/
void
CKjRedisWorkQueue::Finish() {
	//
	// Set done flag and notify.
	//
	_done = true;

	// join
//  	if (_pipeThread.thread->joinable()) {
// 		_pipeThread.thread->join();
//  	}
}

//------------------------------------------------------------------------------
/**

*/
void
CKjRedisWorkQueue::Start() {
	//
	_pipeThread = _pipeContext->NewPipeThread(
		[this](kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
		//
		Run(ioProvider, stream, waitScope);
	});
}

//------------------------------------------------------------------------------
/**

*/
void
CKjRedisWorkQueue::Run(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
	//
	redis_thread_worker_t worker;
	worker._tioContext = kj::refcounted<KjSimpleThreadIoContext>(ioProvider, stream, waitScope);
	worker._tasks = worker._tioContext->CreateTaskSet();
	worker._conn = kj::heap<KjRedisConnection>(kj::addRef(*worker._tioContext));

	auto paf = kj::newPromiseAndFulfiller<void>();
	worker._conn->Open(_refParam);

	// check quit
	worker._tasks->add(
		check_quit_loop(
			*this,
			worker,
			paf.fulfiller.get()),
		"check_quit_loop");

	// read pipe
	worker._tasks->add(
		read_pipe_loop(
			*this,
			worker,
			stream),
		"read_pipe_loop");

	//
	paf.promise.wait(worker._tioContext->GetWaitScope());

	// thread dispose
	worker._conn->Close();
}

/** -- EOF -- **/