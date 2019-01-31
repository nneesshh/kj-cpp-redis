//------------------------------------------------------------------------------
//  KjRedisTcpConn.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjRedisTcpConn.hpp"

#include <assert.h>

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID

#include "servercore/capnp/kj/debug.h"

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

#include "../RedisRootContextDef.hpp"

#define KJ_TCP_CONNECTION_READ_SIZE				4096
#define KJ_TCP_CONNECTION_READ_RESERVE_SIZE		1024 * 8

//------------------------------------------------------------------------------
/**
//! ctor & dtor
*/
KjRedisTcpConn::KjRedisTcpConn(kj::Own<KjPipeEndpointIoContext> endpointContext, uint64_t connid)
	: _endpointContext(kj::mv(endpointContext))
	, _connid(connid) {
	
	_connAttach._readSize = KJ_TCP_CONNECTION_READ_SIZE;
	_bb = bip_buf_create(KJ_TCP_CONNECTION_READ_RESERVE_SIZE);
}

//------------------------------------------------------------------------------
/**

*/
KjRedisTcpConn::~KjRedisTcpConn() {
	FlushStream();
	bip_buf_destroy(_bb);
}

//------------------------------------------------------------------------------
/**
//! comparison operator
*/
bool
KjRedisTcpConn::operator==(const KjRedisTcpConn& rhs) const {
	return _connid == rhs._connid;
}

//------------------------------------------------------------------------------
/**

*/
bool
KjRedisTcpConn::operator!=(const KjRedisTcpConn& rhs) const {
	return !operator==(rhs);
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisTcpConn::Connect(
	kj::StringPtr host,
	kj::uint port,
	CONNECT_CB connectCb,
	DISCONNECT_CB disconnectCb) {

	auto paf = kj::newPromiseAndFulfiller<void>();
	_disconnectPromise = paf.promise.fork();
	_disconnectFulfiller = kj::mv(paf.fulfiller);

	_connAttach._connectCb = connectCb;
	_connAttach._disconnectCb = disconnectCb;

	_host = host;
	_port = port;

	auto p1 = _endpointContext->GetNetwork().parseAddress(_host, _port)
		.then([this](kj::Own<kj::NetworkAddress>&& addr) {

		_addr = kj::mv(addr);
		return StartConnect();
	}).exclusiveJoin(DisconnectWatcher());
	return p1;
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisTcpConn::Disconnect() {
	//
	if (_bConnected) {
		_bConnected = false;

		if (_connAttach._disconnectCb)
			_connAttach._disconnectCb(*this, _connid);
	}

	// stream disconnect means disposed
	if (!_bDisposed) {
		_bDisposed = true;

		// flush stream
		FlushStream();
	}
}

//------------------------------------------------------------------------------
/**

*/
void
KjRedisTcpConn::FlushStream() {

	if (_stream) {
		// flush stream at once
		/*try {
			_stream->abortRead();
			_stream->shutdownWrite();
		}
		catch (std::exception& e) {
			StdLog *pLog = redis_get_log();
			if (pLog)
				pLog->logprint(LOG_LEVEL_NOTICE, "[KjRedisTcpConn::FlushStream()] abortRead or shutdownWrite exception -- what(%s)!!!"
					, e.what());

			fprintf(stderr, "[KjRedisTcpConn::FlushStream()] abortRead or shutdownWrite exception -- what(%s)!!!"
				, e.what());
		}*/
		_stream = nullptr;
	}

	if (_disconnectFulfiller) {
		_disconnectFulfiller->fulfill();
		_disconnectFulfiller = nullptr;
	}

	// clear bb
	bip_buf_reset(_bb);
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisTcpConn::Reconnect() {

	assert(!_bDisposed);

	if (!_disconnectFulfiller) {
		auto paf = kj::newPromiseAndFulfiller<void>();
		_disconnectPromise = paf.promise.fork();
		_disconnectFulfiller = kj::mv(paf.fulfiller);
	}

	return StartConnect()
		.exclusiveJoin(DisconnectWatcher());
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisTcpConn::StartReadOp(const READ_CB& readCb) {
	_connAttach._readCb = readCb;
	return AsyncReadLoop()
		.exclusiveJoin(DisconnectWatcher());
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisTcpConn::StartConnect() {

	return _addr->connect()
		.then([this](kj::Own<kj::AsyncIoStream>&& stream) {

		_stream = kj::mv(stream);
		_bConnected = true;

		StdLog *pLog = redis_get_log();
		if (pLog)
			pLog->logprint(LOG_LEVEL_NOTICE, "[KjRedisTcpConn::StartConnect()] Connect ok -- connid(%08llu)host(%s)port(%d).\n",
				GetConnId(), GetHost().cStr(), GetPort());

// 		fprintf(stderr, "[KjRedisTcpConn::StartConnect()] Connect ok -- connid(%08llu)host(%s)port(%d).\n",
// 			GetConnId(), GetHost().cStr(), GetPort());
		
		if (_connAttach._connectCb) {
			_connAttach._connectCb(*this, _connid);
		}

	});
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjRedisTcpConn::AsyncReadLoop() {

	size_t buflen = _connAttach._readSize;
	char *bufbase = bip_buf_force_reserve(_bb, buflen);

	assert(bufbase);

	return _stream->read(bufbase, 1, buflen)
		.then([this](size_t amount) {
		//
		bip_buf_commit(_bb, (int)amount);

		if (_connAttach._readCb) {
			_connAttach._readCb(*this, *_bb);
		}
		return AsyncReadLoop();
	});
}

/* -- EOF -- */