#pragma once
//------------------------------------------------------------------------------
/**
	@class KjRedisClientConn

	(C) 2016 n.lee
*/
#include "servercore/base/IServerCore.h"
#include "base/IRedisService.h"

#include "KjRedisTcpConn.hpp"
#include "KjReplyBuilder.hpp"
#include "KjRedisClientWorkQueue.hpp"

//------------------------------------------------------------------------------
/**
	@brief KjRedisClientConn

//!
//! redis connection wrapper handling client protocol
//!
*/
class KjRedisClientConn : public kj::Refcounted, public kj::TaskSet::ErrorHandler {
public:
	//! ctor & dtor
	explicit KjRedisClientConn(kj::Own<KjPipeEndpointIoContext> endpointContext, redis_stub_param_t& param);
	~KjRedisClientConn();

	//! copy ctor & assignment operator
	KjRedisClientConn(const KjRedisClientConn&) = delete;
	KjRedisClientConn& operator=(const KjRedisClientConn&) = delete;

	virtual void OnClientConnect(KjRedisTcpConn&, uint64_t);
	virtual void OnClientDisconnect(KjRedisTcpConn&, uint64_t);
	virtual void OnClientReceive(KjRedisTcpConn&, bip_buf_t& bb);

public:
	void Open(redis_stub_param_t& param);
	void Close();

	void Connect(const std::string& host, unsigned short port);
	void Disconnect();

	bool IsConnected() const {
		return _kjconn.IsConnected();
	}

	//! send cmd pipeline
	KjRedisClientConn& Send(redis_cmd_pipepline_t& cp) {
		cp._state = redis_cmd_pipepline_t::SENDING;
		_dqCommon.emplace_back(std::move(cp));
		return *this;
	}

	//! commit pipelined transaction
	KjRedisClientConn& Commit();

	//! queued cmd pipeline size
	size_t UncommittedSize() {
		return _dqCommon.size();
	}

private:
	//!
	void Init();

	//! 
	void DelayReconnect();

	//! 
	kj::Promise<void> CommitLoop();

	//! 
	void taskFailed(kj::Exception&& exception) override;

private:
	kj::Own<KjPipeEndpointIoContext> _endpointContext;
	redis_stub_param_t& _refParam;
	kj::Own<kj::TaskSet> _tsCommon;

	//! redis service cmd pipelines need to be commit
	std::deque<redis_cmd_pipepline_t> _dqInit;
	std::deque<redis_cmd_pipepline_t> _dqCommon;

	int _committing_num = 0;

	KjRedisTcpConn _kjconn;
	KjReplyBuilder _builder;
	
	bool _bDelayReconnecting = false;
};

/*EOF*/