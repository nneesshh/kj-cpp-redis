//------------------------------------------------------------------------------
//  KjRedisClientWorkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjRedisClientWorkQueue.hpp"

#include "io/KjRedisClientConn.hpp"

struct redis_thread_worker_t {
	kj::Own<KjSimpleThreadIoContext>	_tioContext;
	kj::Own<kj::TaskSet>				_tasks;
	kj::Own<KjRedisClientConn>			_conn;
};

static kj::Promise<void>
check_quit_loop(CKjRedisClientWorkQueue& q, redis_thread_worker_t& worker, kj::PromiseFulfiller<void> *fulfiller) {
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
read_pipe_loop(CKjRedisClientWorkQueue& q, redis_thread_worker_t& worker, kj::AsyncIoStream& stream) {

	if (!q.IsDone()) {
		return stream.tryRead(&q._opCodeRecvBuf, 1, 1024)
			.then([&q, &worker, &stream](size_t amount) {
			//
			// Get next work item.
			//
			int nCount = 0;
			while (q.Callbacks().try_dequeue(q._opCmd)) {
				worker._conn->Send(q._opCmd);
				++nCount;
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
CKjRedisClientWorkQueue::CKjRedisClientWorkQueue(kj::Own<KjSimpleIoContext> rootContext, redis_stub_param_t& param)
	: _rootContext(kj::addRef(*rootContext))
	, _refParam(param)
	, _callbacks(256) {
	//
	Start();
}

//------------------------------------------------------------------------------
/**

*/
CKjRedisClientWorkQueue::~CKjRedisClientWorkQueue() {
	_pipeThread.pipe = nullptr;
	_pipeThread.thread = nullptr;
}

//------------------------------------------------------------------------------
/**

*/
bool
CKjRedisClientWorkQueue::Add(cmd_pipepline_t&& cmd) {

	if (_done) {
		// error
		fprintf(stderr, "[CKjRedisClientWorkQueue::Add()] can't enqueue, callback is dropped!!!");
		return false;
	}

	//
	// Add work item.
	//
	if (!_callbacks.enqueue(std::move(cmd))) {
		// error
		fprintf(stderr, "[CKjRedisClientWorkQueue::Add()] enqueue failed, callback is dropped!!!");
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
CKjRedisClientWorkQueue::Finish() {
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
CKjRedisClientWorkQueue::Start() {
	//
	_pipeThread = _rootContext->NewPipeThread(
		[this](kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
		//
		Run(ioProvider, stream, waitScope, _refParam._mapScript);
	});
}

//------------------------------------------------------------------------------
/**

*/
void
CKjRedisClientWorkQueue::Run(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope, const std::map<std::string, std::string>& mapScript) {
	//
	redis_thread_worker_t worker;
	worker._tioContext = kj::refcounted<KjSimpleThreadIoContext>(ioProvider, stream, waitScope);
	worker._tasks = worker._tioContext->CreateTaskSet(*this);
	worker._conn = kj::heap<KjRedisClientConn>(kj::addRef(*worker._tioContext), _refParam);

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

//------------------------------------------------------------------------------
/**

*/
void
CKjRedisClientWorkQueue::taskFailed(kj::Exception&& exception) {
	fprintf(stderr, "[CKjRedisClientWorkQueue::taskFailed()] desc(%s) -- pause!!!\n", exception.getDescription().cStr());
	system("pause");
	kj::throwFatalException(kj::mv(exception));
}

/** -- EOF -- **/