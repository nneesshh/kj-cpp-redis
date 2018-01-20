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
	kj::PromiseForResult<Func, void> EvalLater(Func func) {
		return kj::evalLater(func);
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

	kj::AsyncIoProvider::PipeThread NewPipeThread(kj::Function<void(kj::AsyncIoProvider&, kj::AsyncIoStream&, kj::WaitScope&)>&& startFunc) {
		return _ioProvider.newPipeThread(kj::mv(startFunc));
	}

private:
	void taskFailed(kj::Exception&& exception) override {
		fprintf(stderr, "[KjSimpleThreadIoContext::taskFailed()] desc(%s) -- pause!!!\n", exception.getDescription().cStr());
		system("pause");
		kj::throwFatalException(kj::mv(exception));
	}

	kj::AsyncIoProvider& _ioProvider;
	kj::AsyncIoStream& _stream;
	kj::WaitScope& _waitScope;
};

/*EOF*/