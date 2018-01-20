//------------------------------------------------------------------------------
//  KjSimpleIoContext.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjSimpleIoContext.hpp"

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID
#include <kj/debug.h>

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

#ifdef _MSC_VER
#ifdef _DEBUG
#define new   new(_NORMAL_BLOCK, __FILE__,__LINE__)
#endif
#endif

// test
class KjDummyEventPort : public kj::EventPort {
public:
	bool runnable = false;
	int callCount = 0;

	bool wait() override {
		KJ_FAIL_ASSERT("Nothing to wait for.");
	}

	bool poll() override { return false; }
	void setRunnable(bool runnable) override {
		this->runnable = runnable;
		++callCount;
	}
};

/* -- EOF -- */