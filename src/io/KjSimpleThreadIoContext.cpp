//------------------------------------------------------------------------------
//  KjSimpleThreadIoContext.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjSimpleThreadIoContext.hpp"

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

//------------------------------------------------------------------------------
/**

*/
KjSimpleThreadIoContext::KjSimpleThreadIoContext(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope)
	: _ioProvider(ioProvider)
	, _stream(stream)
	, _waitScope(waitScope)
	, _defaultTasks(CreateTaskSet()) {

}

//------------------------------------------------------------------------------
/**

*/
void
KjSimpleThreadIoContext::taskFailed(kj::Exception&& exception) {
	fprintf(stderr, "[KjSimpleThreadIoContext::taskFailed()] desc(%s) -- pause!!!\n", exception.getDescription().cStr());
	system("pause");
	kj::throwFatalException(kj::mv(exception));
}

/* -- EOF -- */