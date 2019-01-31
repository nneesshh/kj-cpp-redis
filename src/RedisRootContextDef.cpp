#include "RedisRootContextDef.hpp"

static IServerCore *s_servercore;

void
redis_init_servercore(void *servercore) {
	if (!s_servercore) {
		s_servercore = static_cast<IServerCore *>(servercore);
	}
}

void
redis_cleanup_servercore() {
	s_servercore = nullptr;
}

IServerCore *
redis_get_servercore() {
	return s_servercore;
}

StdLog *
redis_get_log() {
	return s_servercore->GetLogHandler();
}

/* -- EOF -- */