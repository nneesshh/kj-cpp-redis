//------------------------------------------------------------------------------
//  KjRedisSubscriberWorkQueue.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjRedisSubscriberWorkQueue.hpp"

#include "KjRedisSubscriberConn.hpp"

struct redis_subscriber_thread_worker_t {
	kj::Own<KjSimpleThreadIoContext>	_tioContext;
	kj::Own<kj::TaskSet>				_tasks;
	kj::Own<KjRedisSubscriberConn>		_conn;
};

static kj::Promise<void>
check_quit_loop(CKjRedisSubscriberWorkQueue& q, redis_subscriber_thread_worker_t& worker, kj::PromiseFulfiller<void> *fulfiller) {
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
read_pipe_loop(CKjRedisSubscriberWorkQueue& q, redis_subscriber_thread_worker_t& worker, kj::AsyncIoStream& stream) {

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
CKjRedisSubscriberWorkQueue::CKjRedisSubscriberWorkQueue(
	kj::Own<KjSimpleIoContext> rootContext,
	redis_stub_param_t& param,
	const std::function<void(std::string&, std::string&)>& workCb1,
	const std::function<void(std::string&, std::string&, std::string&)>& workCb2)
	:_rootContext(kj::addRef(*rootContext))
	, _refParam(param)
	, _refWorkCb1(workCb1)
	, _refWorkCb2(workCb2)
	, _callbacks(256) {
	//
	Start();
}

//------------------------------------------------------------------------------
/**

*/
CKjRedisSubscriberWorkQueue::~CKjRedisSubscriberWorkQueue() {
	_pipeThread.pipe = nullptr;
	_pipeThread.thread = nullptr;
}

//------------------------------------------------------------------------------
/**

*/
bool
CKjRedisSubscriberWorkQueue::Add(cmd_pipepline_t&& cmd) {

	if (_done) {
		// error
		fprintf(stderr, "[CKjRedisSubscriberWorkQueue::Add()] can't enqueue, callback is dropped!!!");
		return false;
	}

	//
	// Add work item.
	//
	if (!_callbacks.enqueue(std::move(cmd))) {
		// error
		fprintf(stderr, "[CKjRedisSubscriberWorkQueue::Add()] enqueue failed, callback is dropped!!!");
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
CKjRedisSubscriberWorkQueue::Finish() {
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
CKjRedisSubscriberWorkQueue::Start() {
	//
	_pipeThread = _rootContext->NewPipeThread(
		[this](kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
		//
		Run(ioProvider, stream, waitScope);
	});
}

//------------------------------------------------------------------------------
/**

*/
void
CKjRedisSubscriberWorkQueue::Run(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
	//
	redis_subscriber_thread_worker_t worker;
	worker._tioContext = kj::refcounted<KjSimpleThreadIoContext>(ioProvider, stream, waitScope);
	worker._tasks = worker._tioContext->CreateTaskSet(*this);
	worker._conn = kj::heap<KjRedisSubscriberConn>(kj::addRef(*worker._tioContext), _refParam, _refWorkCb1, _refWorkCb2);

	auto paf = kj::newPromiseAndFulfiller<void>();
	worker._conn->Open();

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
CKjRedisSubscriberWorkQueue::taskFailed(kj::Exception&& exception) {
	fprintf(stderr, "[CKjRedisSubscriberWorkQueue::taskFailed()] desc(%s) -- pause!!!\n", exception.getDescription().cStr());
	system("pause");
	kj::throwFatalException(kj::mv(exception));
}

/** -- EOF -- **/