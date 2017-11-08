//------------------------------------------------------------------------------
//  KjRedisConnection.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID

#include <kj/debug.h>

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

//! utils
#include "base/RedisError.h"
#include "KjRedisConnection.hpp"

//------------------------------------------------------------------------------
/**

*/
KjRedisConnection::KjRedisConnection(kj::Own<KjSimpleThreadIoContext> tioContext)
	: _tioContext(kj::mv(tioContext))
	, _tasks(_tioContext->CreateTaskSet(*this))
	, _kjclient(kj::addRef(*_tioContext), __CPP_REDIS_READ_SIZE) {

}

//------------------------------------------------------------------------------
/**
*/
KjRedisConnection::~KjRedisConnection() {

	_kjclient.Disconnect();
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::OnClientConnect(KjTcpClient& kjclient, uint64_t connid) {
	// start read
	auto p1 = kjclient.StartReadOp(
		std::bind(&KjRedisConnection::OnClientReceive,
			this,
			std::placeholders::_1,
			std::placeholders::_2));

	_tasks->add(kj::mv(p1), "startreadop");
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::OnClientDisconnect(KjTcpClient& kjclient, uint64_t connid) {

}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::OnClientReceive(KjTcpClient& kjclient, bip_buf_t& bbuf) {

	try {
		_builder.ProcessInput(bbuf);
	}
	catch (const CRedisError&) {
		return;
	}

	while (_builder.IsReplyAvailable()) {

		auto& reply = _builder.GetFront();

		if (reply.is_error()) {

			std::string sDesc = "[reply error!!!] ";
			sDesc += reply.as_string();
			fprintf(stderr, sDesc.c_str());
			system("pause");
			throw CRedisError(sDesc.c_str());
		}
		
		if (_sending_num <= 0
			|| _rscmds.size() <= 0) {

			std::string sDesc = "[got reply but cmd is discarded!!!] ";
			fprintf(stderr, sDesc.c_str());
			system("pause");
			throw CRedisError(sDesc.c_str());
		}

		{
			auto& cmd = _rscmds.front();
			if (IRedisService::cmd_t::REDIS_CMD_STATE_SENDING == cmd._state) {
				cmd._state = IRedisService::cmd_t::REDIS_CMD_STATE_PROCESSING;
				++cmd._processed_num;
			}
			else if (IRedisService::cmd_t::REDIS_CMD_STATE_PROCESSING == cmd._state) {
				++cmd._processed_num;
			}
			else {
				std::string sDesc = "cmd is corrupted!!!";
				fprintf(stderr, sDesc.c_str());
				system("pause");
				throw CRedisError(sDesc.c_str());
			}

			if (cmd._processed_num >= cmd._built_num) {
				cmd._state = IRedisService::cmd_t::REDIS_CMD_STATE_PROCESS_OVER;

				//
				if (cmd._reply_cb)
					cmd._reply_cb(std::move(reply));

				//
				if (cmd._dispose_cb)
					cmd._dispose_cb();

				//
				_rscmds.pop_front();
				--_sending_num;
			}
		}

		//
		_builder.PopFront();
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::Open(redis_stub_param_t& param) {
	Connect(param._ip, param._port, [](KjRedisConnection& ) {
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!disconnected");
	});

}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::Close() {
	
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::Connect(
	const std::string& host,
	unsigned short port,
	const disconnection_handler_t& disconnection_handler) {

	auto p1 = _kjclient.Connect(
		host,
		port,
		std::bind(&KjRedisConnection::OnClientConnect, this, std::placeholders::_1, std::placeholders::_2),
		std::bind(&KjRedisConnection::OnClientDisconnect, this, std::placeholders::_1, std::placeholders::_2));

	_tasks->add(kj::mv(p1), "kjclient connect");

	_disconnection_handler = disconnection_handler;
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::Disconnect() {
	
	_kjclient.Disconnect();
}

//------------------------------------------------------------------------------
/**

*/
bool
KjRedisConnection::IsConnected() {
	return _kjclient.IsConnected();
}

//------------------------------------------------------------------------------
/**

*/
KjRedisConnection&
KjRedisConnection::Send(IRedisService::cmd_t& cmd) {
	cmd._state = IRedisService::cmd_t::REDIS_CMD_STATE_QUEUEING;
	_rscmds.push_back(std::move(cmd));
	return *this;
}

//------------------------------------------------------------------------------
/**
//! commit pipelined transaction
*/
KjRedisConnection&
KjRedisConnection::Commit() {
	if (_rscmds.size() > 0) {
		_tasks->add(CommitLoop(), "commit loop");
	}
	return *this;
}

//------------------------------------------------------------------------------
/**
//! commit loop
*/
kj::Promise<void>
KjRedisConnection::CommitLoop() {
	if (IsConnected()) {
		for (auto& cmd : _rscmds) {
			if (IRedisService::cmd_t::REDIS_CMD_STATE_QUEUEING == cmd._state) {

				_tasks->add(_kjclient.Write(cmd._commands.c_str(), cmd._commands.length()), "kjclient write commands");
				
				cmd._state = IRedisService::cmd_t::REDIS_CMD_STATE_SENDING;
				++_sending_num;
			}
		}
		
		// commit over
		return kj::READY_NOW;
	}
	else {
		auto p1 = _tioContext->AfterDelay(1 * kj::SECONDS, "delay and commit loop")
			.then([this]() {

			return CommitLoop();
		});
		return kj::mv(p1);
	}
}

/** -- EOF -- **/