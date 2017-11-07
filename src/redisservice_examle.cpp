#include "RedisService.h"

int main() {
	redis_stub_param_t param;
	param._ip = "127.0.0.1";
	param._port = 6379;

	CRedisService *pRedisService = new CRedisService(&param);
	pRedisService->HSet("key1", "field1", "It is a test string");
	pRedisService->BlockingCommit();

	pRedisService->HGet("key1", "field1");
	CRedisReply reply1 = pRedisService->BlockingCommit();

	// receive for asynchronous
	while (true) {
		pRedisService->RunOnce();
	}

	delete pRedisService;
	return 0;
}