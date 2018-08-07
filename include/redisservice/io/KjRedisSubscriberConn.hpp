#pragma once
//------------------------------------------------------------------------------
/**
@class KjRedisSubscriberConn

(C) 2016 n.lee
*/
#include "base/IRedisService.h"

#include "KjTcpConnection.hpp"
#include "KjReplyBuilder.hpp"
#include "KjRedisSubscriberWorkQueue.hpp"

//------------------------------------------------------------------------------
/**
@brief KjRedisSubscriberConn

//!
//! redis connection wrapper handling subscriber protocol
//!
*/
class KjRedisSubscriberConn : public kj::Refcounted, public kj::TaskSet::ErrorHandler {
public:
	//! ctor & dtor
	explicit KjRedisSubscriberConn(
		kj::Own<KjSimpleThreadIoContext> tioContext,
		redis_stub_param_t& param,
		const std::function<void(std::string&, std::string&)>& workCb1,
		const std::function<void(std::string&, std::string&, std::string&)>& workCb2);

	~KjRedisSubscriberConn();

	//! copy ctor & assignment operator
	KjRedisSubscriberConn(const KjRedisSubscriberConn&) = delete;
	KjRedisSubscriberConn& operator=(const KjRedisSubscriberConn&) = delete;

	virtual void OnClientConnect(KjTcpConnection&, uint64_t);
	virtual void OnClientDisconnect(KjTcpConnection&, uint64_t);
	virtual void OnClientReceive(KjTcpConnection&, bip_buf_t& bb);

public:
	void Open();
	void Close();

	void Connect(const std::string& host, unsigned short port);
	void Disconnect();

	bool IsConnected() const {
		return _kjconn.IsConnected();
	}

	//! send cmd pipeline
	KjRedisSubscriberConn& Send(redis_cmd_pipepline_t& cp) {
		cp._state = redis_cmd_pipepline_t::SENDING;
		_dqCommon.emplace_back(std::move(cp));
		return *this;
	}

	//! commit pipelined transaction
	KjRedisSubscriberConn& Commit();

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
	const std::function<void(std::string&, std::string&)>& _refWorkCb1;
	const std::function<void(std::string&, std::string&, std::string&)>& _refWorkCb2;

	//! redis service cmd pipelines need to be commit
	std::deque<redis_cmd_pipepline_t> _dqInit;
	std::deque<redis_cmd_pipepline_t> _dqCommon;

	int _committing_num = 0;

	KjTcpConnection _kjconn;
	KjReplyBuilder _builder;

	bool _bDelayReconnecting = false;
};

/*EOF*/