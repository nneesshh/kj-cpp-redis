#pragma once
//------------------------------------------------------------------------------
/**
@class CKjSimpleIoContext

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

class KjSimpleIoContext : public kj::Refcounted, public kj::TaskSet::ErrorHandler {
public:
	KjSimpleIoContext() : _ioContext(kj::setupAsyncIo()) {}
	~KjSimpleIoContext() noexcept(false) = default;

	kj::WaitScope&				GetWaitScope() {
		return _ioContext.waitScope;
	}

	kj::Network&				GetNetwork() {
		return _ioContext.provider->getNetwork();
	}

	kj::Timer&					GetTimer() {
		return _ioContext.provider->getTimer();
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
		return _ioContext.provider->newPipeThread(kj::mv(startFunc));
	}

private:
	void taskFailed(kj::Exception&& exception) override {
		fprintf(stderr, "[KjSimpleIoContext::taskFailed()] desc(%s) -- pause!!!\n", exception.getDescription().cStr());
		system("pause");
		kj::throwFatalException(kj::mv(exception));
	}

	kj::AsyncIoContext _ioContext;
};

/*EOF*/