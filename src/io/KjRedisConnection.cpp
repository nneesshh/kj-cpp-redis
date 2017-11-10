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
KjRedisConnection::OnClientConnect(KjTcpClient&, uint64_t connid) {
	// start read
	auto p1 = _kjclient.StartReadOp(
		std::bind(&KjRedisConnection::OnClientReceive,
			this,
			std::placeholders::_1,
			std::placeholders::_2)
	);
	_tasks->add(kj::mv(p1), "kjclient start read op");
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::OnClientDisconnect(KjTcpClient&, uint64_t connid) {

}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisConnection::OnClientReceive(KjTcpClient&, bip_buf_t& bbuf) {

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
		
		if (_committing_num <= 0
			|| _rscps.size() <= 0) {

			std::string sDesc = "[got reply but cmd is discarded!!!] ";
			fprintf(stderr, sDesc.c_str());
			system("pause");
			throw CRedisError(sDesc.c_str());
		}

		{
			auto& cp = _rscps.front();
			if (IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_COMMITING == cp._state) {
				cp._state = IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_PROCESSING;
				++cp._processed_num;
			}
			else if (IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_PROCESSING == cp._state) {
				++cp._processed_num;
			}
			else {
				std::string sDesc = "cmd pipeline is corrupted!!!";
				fprintf(stderr, sDesc.c_str());
				system("pause");
				throw CRedisError(sDesc.c_str());
			}

			if (cp._processed_num >= cp._built_num) {
				cp._state = IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_PROCESS_OVER;

				//
				if (cp._reply_cb)
					cp._reply_cb(std::move(reply));

				//
				if (cp._dispose_cb)
					cp._dispose_cb();

				//
				_rscps.pop_front();
				--_committing_num;
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
		fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!disconnected");
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
		std::bind(&KjRedisConnection::OnClientDisconnect, this, std::placeholders::_1, std::placeholders::_2)
	);
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
//! commit loop
*/
void
KjRedisConnection::AutoReconnect() {
	//
	_tasks->add(_kjclient.DelayReconnect(), "kjclient delay reconnect");

	// recommit
	for (auto& cmd : _rscps) {
		// recover sending state
		if (IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_COMMITING == cmd._state) {
			cmd._state = IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_SENDING;
			--_committing_num;
		}
	}
	Commit();
}

//------------------------------------------------------------------------------
/**
//! commit loop
*/
kj::Promise<void>
KjRedisConnection::CommitLoop() {
	if (IsConnected()) {
		for (auto& cmd : _rscps) {
			if (IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_SENDING == cmd._state) {

				_tasks->add(_kjclient.Write(cmd._commands.c_str(), cmd._commands.length()), "kjclient write commands");
				
				cmd._state = IRedisService::cmd_pipepline_t::CMD_PIPELINE_STATE_COMMITING;
				++_committing_num;
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