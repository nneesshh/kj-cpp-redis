#pragma once
//------------------------------------------------------------------------------
/**
@class KjExecCmdOp

(C) 2017 n.lee
*/
#include "kj_redis_client.hpp"

class KjExecCmdOp {
	using rc = cpp_redis::kj_redis_client;
	using rcb_t = std::function<void(kj::Own<CRsReply>)>;
	using func_exec_cmd_t = std::function<rc&(const rcb_t&)>;

public:
	kj::PromiseFulfiller<kj::Own<CRsReply>>& _fulfiller;
	cpp_redis::kj_redis_client& _client;
	
	bool _disposed = false;

	KjExecCmdOp(kj::PromiseFulfiller<kj::Own<CRsReply>>& fulfiller, cpp_redis::kj_redis_client& client, func_exec_cmd_t f) :
		_fulfiller(fulfiller),
		_client(client) {

		f([this](kj::Own<CRsReply> r) {	Fulfill(kj::mv(r));	}).commit();
	}

	~KjExecCmdOp() {
		if (!_disposed) {

		}
	}

	void Fulfill(kj::Own<CRsReply> r) {
		_fulfiller.fulfill(kj::mv(r));
		_disposed = true;
	}

};

/*EOF*/