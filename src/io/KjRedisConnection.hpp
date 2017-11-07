#pragma once
//------------------------------------------------------------------------------
/**
@class KjRedisConnection

(C) 2016 n.lee
*/
#include "base/IRedisService.h"

#include "KjTcpClient.hpp"
#include "KjReplyBuilder.hpp"

#ifndef __CPP_REDIS_READ_SIZE
#define __CPP_REDIS_READ_SIZE 4096
#endif /* __CPP_REDIS_READ_SIZE */

//------------------------------------------------------------------------------
/**
@brief KjRedisConnection
*/
class KjRedisConnection : public kj::TaskSet::ErrorHandler {
public:
	//! ctor & dtor
	KjRedisConnection(kj::Own<KjSimpleThreadIoContext> tioContext);
	~KjRedisConnection();

	using reply_callback_t = std::function<void(KjRedisConnection&, kj::Own<CRedisReply>)>;
	using disconnection_handler_t = std::function<void(KjRedisConnection&)>;

	//! copy ctor & assignment operator
	KjRedisConnection(const KjRedisConnection&) = delete;
	KjRedisConnection& operator=(const KjRedisConnection&) = delete;

	virtual void OnClientConnect(KjTcpClient&, uint64_t);
	virtual void OnClientDisconnect(KjTcpClient&, uint64_t);
	virtual void OnClientReceive(KjTcpClient&, bip_buf_t& bbuf);

public:
	void Open(redis_stub_param_t& param);
	void Close();

	void Connect(
		const std::string& host = "127.0.0.1",
		unsigned short port = 6379,
		const disconnection_handler_t& disconnection_handler = nullptr);

	void Disconnect();
	bool IsConnected();

	//! send cmd
	KjRedisConnection& Send(IRedisService::cmd_t& cmd);

	//! commit pipelined transaction
	KjRedisConnection& Commit();

	//! queued cmd size
	size_t UncommittedSize() {
		return _rscmds.size();
	}

private:
	//! 
	void taskFailed(kj::Exception&& exception) override {
		fprintf(stderr, "[KjRedisConnection::taskFailed()] desc(%s) -- pause!!!\n", exception.getDescription().cStr());
		_tasks->add(_kjclient.DelayReconnect(), "kjclient delay reconnect");
	}

	//! 
	kj::Promise<void> CommitLoop();

private:
	//! tcp client for redis connection
	kj::Own<KjSimpleThreadIoContext> _tioContext;
	kj::Own<kj::TaskSet> _tasks;
	KjTcpClient _kjclient;

	//! redis service cmds need to be commit
	std::deque<IRedisService::cmd_t> _rscmds;

	//! user defined disconnection handler
	disconnection_handler_t _disconnection_handler = nullptr;

	//! reply builder
	cpp_redis::builders::KjReplyBuilder _builder;

	int _sending_num = 0;
};

/*EOF*/
