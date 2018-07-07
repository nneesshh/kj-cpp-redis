#pragma once
//------------------------------------------------------------------------------
/**
	@class KjRedisClientConn

	(C) 2016 n.lee
*/
#include "base/IRedisService.h"

#include "KjTcpConnection.hpp"
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
	explicit KjRedisClientConn(kj::Own<KjSimpleThreadIoContext> tioContext, redis_stub_param_t& param);
	~KjRedisClientConn();

	//! copy ctor & assignment operator
	KjRedisClientConn(const KjRedisClientConn&) = delete;
	KjRedisClientConn& operator=(const KjRedisClientConn&) = delete;

	virtual void OnClientConnect(KjTcpConnection&, uint64_t);
	virtual void OnClientDisconnect(KjTcpConnection&, uint64_t);
	virtual void OnClientReceive(KjTcpConnection&, bip_buf_t& bbuf);

public:
	void Open(redis_stub_param_t& param);
	void Close();

	void Connect(const std::string& host, unsigned short port);
	void Disconnect();

	bool IsConnected() const {
		return _kjconn.IsConnected();
	}

	//! send cmd pipeline
	KjRedisClientConn& Send(CKjRedisClientWorkQueue::cmd_pipepline_t& cp) {
		cp._state = CKjRedisClientWorkQueue::cmd_pipepline_t::CMD_PIPELINE_STATE_SENDING;
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
	kj::Own<KjSimpleThreadIoContext> _tioContext;
	kj::Own<kj::TaskSet> _tasks;
	
	redis_stub_param_t& _refParam;

	//! redis service cmd pipelines need to be commit
	std::deque<CKjRedisClientWorkQueue::cmd_pipepline_t> _dqInit;
	std::deque<CKjRedisClientWorkQueue::cmd_pipepline_t> _dqCommon;

	int _committing_num = 0;

	KjTcpConnection _kjconn;
	cpp_redis::builders::KjReplyBuilder _builder;
	
	bool _bDelayReconnecting = false;
};

/*EOF*/