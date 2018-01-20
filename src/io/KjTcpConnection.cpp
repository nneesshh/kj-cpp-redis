//------------------------------------------------------------------------------
//  KjTcpConnection.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjTcpConnection.hpp"

#include <assert.h>

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID

#include <kj/debug.h>

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

#define KJ_TCP_CONNECTION_READ_SIZE				4096
#define KJ_TCP_CONNECTION_READ_RESERVE_SIZE		1024 * 64

//------------------------------------------------------------------------------
/**
//! ctor & dtor
*/
KjTcpConnection::KjTcpConnection(kj::Own<KjSimpleThreadIoContext> tioContext, uint64_t connid)
	: _tioContext(kj::mv(tioContext))
	, _connid(connid) {
	
	_connAttach._readSize = KJ_TCP_CONNECTION_READ_SIZE;
	_bbuf = bip_buf_create(KJ_TCP_CONNECTION_READ_RESERVE_SIZE);
}

//------------------------------------------------------------------------------
/**

*/
KjTcpConnection::~KjTcpConnection() {
	Disconnect();

	bip_buf_destroy(_bbuf);
}

//------------------------------------------------------------------------------
/**
//! comparison operator
*/
bool
KjTcpConnection::operator==(const KjTcpConnection& rhs) const {
	return _connid == rhs._connid;
}

//------------------------------------------------------------------------------
/**

*/
bool
KjTcpConnection::operator!=(const KjTcpConnection& rhs) const {
	return !operator==(rhs);
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::Connect(
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

	auto p1 = _tioContext->GetNetwork().parseAddress(_host, _port)
		.then([this](kj::Own<kj::NetworkAddress>&& addr) {

		_addr = kj::mv(addr);

		return StartConnect();
	}).exclusiveJoin(DisconnectWatcher());
	return kj::mv(p1);
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::Write(const void* buffer, size_t size) {
	auto p1 = _stream->write(buffer, size)
		.exclusiveJoin(DisconnectWatcher());
	return kj::mv(p1);
}

//------------------------------------------------------------------------------
/**

*/
void
KjTcpConnection::Disconnect() {
	if (_bIsConnected) {
		//
		_bIsConnected = false;

		//
		OnDisconnect();

		//
		_stream->abortRead();
		_stream->shutdownWrite();
		_disconnectFulfiller->fulfill();
	}
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::StartReadOp(const READ_CB& readCb) {
	_connAttach._readCb = readCb;
	return AsyncReadLoop()
		.exclusiveJoin(DisconnectWatcher());
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::DelayReconnect() {

	if (_bIsConnected) {
		//
		_bIsConnected = false;

		//
		OnDisconnect();
	}
	return Reconnect();
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::StartConnect() {

	return _addr->connect()
		.then([this](kj::Own<kj::AsyncIoStream>&& stream) {

		_stream = kj::mv(stream);
		_bIsConnected = true;

		fprintf(stderr, "Connect ok.\n");
		
		if (_connAttach._connectCb) {
			_connAttach._connectCb(*this, _connid);
		}

	});
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::Reconnect() {
	const unsigned int uDelaySeconds = 3;

	auto paf = kj::newPromiseAndFulfiller<void>();
	_disconnectPromise = paf.promise.fork();
	_disconnectFulfiller = kj::mv(paf.fulfiller);

	if (!_bIsReconnecting) {

		_bIsReconnecting = true;

		fprintf(stderr, "[KjTcpConnection::Reconnect()] host(%s)port(%d), reconnect after (%d) seconds...\n",
			_host.cStr(), _port, uDelaySeconds);

		return _tioContext->AfterDelay(uDelaySeconds * kj::SECONDS, "reconnect")
			.then([this]() {

			_bIsReconnecting = false;
			return StartConnect();
		});
	}
	return kj::READY_NOW;
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpConnection::AsyncReadLoop() {

	size_t buflen = _connAttach._readSize;
	char *bufbase = bip_buf_force_reserve(_bbuf, &buflen);

	assert(bufbase && buflen > 0);

	return _stream->read(bufbase, 1, buflen)
		.then([this](size_t amount) {
		//
		bip_buf_commit(_bbuf, (int)amount);

		if (_connAttach._readCb) {
			_connAttach._readCb(*this, *_bbuf);
		}
		return AsyncReadLoop();
	});
}

/* -- EOF -- */