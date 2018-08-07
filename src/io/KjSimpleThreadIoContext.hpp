#pragma once
//------------------------------------------------------------------------------
/**
@class KjSimpleThreadIoContext

(C) 2016 n.lee
*/
#include <algorithm>

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")

#undef VOID
#include <kj/async.h>
#include <kj/async-io.h>

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

class KjSimpleThreadIoContext : public kj::Refcounted, public kj::TaskSet::ErrorHandler {
public:
	KjSimpleThreadIoContext(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope);
	~KjSimpleThreadIoContext() noexcept(false) = default;

	kj::WaitScope&				GetWaitScope() {
		return _waitScope;
	}

	kj::Network&				GetNetwork() {
		return _ioProvider.getNetwork();
	}

	kj::Timer&					GetTimer() {
		return _ioProvider.getTimer();
	}

	kj::Promise<void>			AfterDelay(kj::Duration delay, const char *timer_name) {
		return GetTimer().afterDelay(delay, timer_name);
	}

	template <typename T>
	kj::Promise<T>				TimeoutAfter(kj::Duration delay, const char *timer_name, kj::Promise<T>&& p) {
		return GetTimer().timeoutAfter(delay, timer_name, kj::mv(p));
	}

	template <typename Func>
	kj::PromiseForResult<Func, void> EvalForResult(Func&& func) {
		return kj::evalLater(kj::mv(func));
	}

	template <typename T>
	kj::ForkedPromise<T>		ForkPromise(kj::Promise<T>&& promise) {
		return promise.fork();
	}

	kj::Own<kj::TaskSet>		CreateTaskSet() {
		return kj::heap<kj::TaskSet>(*this);
	}

	kj::Own<kj::TaskSet>		CreateTaskSet(kj::TaskSet::ErrorHandler& errorHandler) {
		return kj::heap<kj::TaskSet>(errorHandler);
	}

	void                        AddTask(kj::Promise<void>&& promise, const char *name) {
		_defaultTasks->add(kj::mv(promise), name);
	}

	kj::AsyncIoProvider::PipeThread NewPipeThread(kj::Function<void(kj::AsyncIoProvider&, kj::AsyncIoStream&, kj::WaitScope&)>&& startFunc) {
		return _ioProvider.newPipeThread(kj::mv(startFunc));
	}

private:
	void taskFailed(kj::Exception&& exception) override;

	kj::AsyncIoProvider& _ioProvider;
	kj::AsyncIoStream& _stream;
	kj::WaitScope& _waitScope;

	kj::Own<kj::TaskSet> _defaultTasks;

};

/*EOF*/