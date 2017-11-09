#include "RedisService.h"

int main() {
	redis_stub_param_t param;
	param._ip = "127.0.0.1";
	param._port = 6379;

	CRedisService *pRedisService = new CRedisService(&param);

	// receive for asynchronous
	time_t tmLast = time(nullptr);
	int nCount = 0;
	int sn = 0;
	while (true) {
		pRedisService->HSet("key1", "field1", "It is a test string");
		pRedisService->HGet("key1", "field1");
		CRedisReply reply1 = pRedisService->BlockingCommit();

		pRedisService->RunOnce();
		++nCount;

		if (0 == nCount % 10000) {
			time_t tmNow = time(nullptr);
			int nElapsed = (int)(tmNow - tmLast);
			tmLast = tmNow;

			++sn;
			printf("sn=%d, elapsed=%d, reply(%s) -- total=%d\n",
				sn, nElapsed, reply1.as_string().c_str(), nCount);
		}
	}

	delete pRedisService;
	return 0;
}