//------------------------------------------------------------------------------
//  KjTcpClient.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "KjTcpClient.hpp"

#include <assert.h>

#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("VOID")
#undef VOID

#include <kj/debug.h>

#pragma pop_macro("ERROR")
#pragma pop_macro("VOID")

#define KJ_TCP_CLIENT_READ_RESERVE_SIZE	1024 * 64

//------------------------------------------------------------------------------
/**
//! ctor & dtor
*/
KjTcpClient::KjTcpClient(kj::Own<KjSimpleThreadIoContext> tioContext, size_t readSize)
	: _tioContext(kj::mv(tioContext)) {
	
	_connAttach._readSize = readSize;
	_bbuf = bip_buf_create(KJ_TCP_CLIENT_READ_RESERVE_SIZE);

	auto paf = kj::newPromiseAndFulfiller<void>();
	_disconnectPromise = paf.promise.fork();
	_disconnectFulfiller = kj::mv(paf.fulfiller);
}

KjTcpClient::~KjTcpClient() {
	Disconnect();

	bip_buf_destroy(_bbuf);
}

//------------------------------------------------------------------------------
/**
//! comparison operator
*/
bool
KjTcpClient::operator==(const KjTcpClient& rhs) const {
	return _connid == rhs._connid;
}

bool
KjTcpClient::operator!=(const KjTcpClient& rhs) const {
	return !operator==(rhs);
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpClient::Connect(
	kj::StringPtr host,
	kj::uint port,
	CONNECT_CB connectCb,
	DISCONNECT_CB disconnectCb) {

	_connAttach._connectCb = connectCb;
	_connAttach._disconnectCb = disconnectCb;

	fprintf(stderr, "[KjTcpClient::Connect()] parse address, host(%s)port(%d)...\n",
		host.cStr(), port);
	auto p1 = _tioContext->GetNetwork().parseAddress(host, port)
		.then([this](kj::Own<kj::NetworkAddress>&& addr) {
		_addr = kj::mv(addr);

		fprintf(stderr, "[KjTcpClient::Connect() -- parseAddress()] start connect...\n");
		return StartConnect();
	}).exclusiveJoin(_disconnectPromise.addBranch());
	return kj::mv(p1);
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpClient::Write(const void* buffer, size_t size) {
	auto p1 = _stream->write(buffer, size)
		.exclusiveJoin(_disconnectPromise.addBranch());
	return kj::mv(p1);
}

//------------------------------------------------------------------------------
/**

*/
void
KjTcpClient::Disconnect() {
	if (IsConnected()) {
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
KjTcpClient::StartReadOp(READ_CB readCb) {
	_connAttach._readCb = readCb;
	return AsyncReadLoop()
		.exclusiveJoin(_disconnectPromise.addBranch());
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpClient::StartConnect() {
	return _addr->connect()
		.then([this](kj::Own<kj::AsyncIoStream>&& stream) {

		_stream = kj::mv(stream);
		_bIsConnected = true;
		
		fprintf(stderr, "[KjTcpClient::StartConnect() -- then()] call connectCb...\n");

		if (_connAttach._connectCb) {
			_connAttach._connectCb(*this, _connid);
		}
	});
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpClient::Reconnect() {
	const unsigned int uDelaySeconds = 3;

	if (!_bIsReconnecting) {
		_bIsReconnecting = true;
		fprintf(stderr, "[KjTcpClient::Reconnect()] reconnect after (%d) seconds...\n", uDelaySeconds);

		return _tioContext->AfterDelay(uDelaySeconds * kj::SECONDS, "reconnect")
			.then([this]() {
			_bIsReconnecting = false;
			fprintf(stderr, "[KjTcpClient::Reconnect() -- AfterDelay()] start reconnect...\n");
			return StartConnect();
		});
	}
	return kj::READY_NOW;
}

//------------------------------------------------------------------------------
/**

*/
kj::Promise<void>
KjTcpClient::AsyncReadLoop() {
	size_t szreserved = _connAttach._readSize;
	char *bufbase = bip_buf_force_reserve(_bbuf, &szreserved);
	int buflen = szreserved;

	assert(nullptr != bufbase);

	return _stream->read(bufbase, 1, buflen)
		.then([this](size_t amount) {
		//
		char *bufbase;
		int buflen;

		bip_buf_commit(_bbuf, (int)amount);
		bufbase = bip_buf_get_contiguous_block(_bbuf);
		buflen = bip_buf_get_committed_size(_bbuf);
		if (_connAttach._readCb) {
			_connAttach._readCb(*this, *_bbuf);
		}
		return AsyncReadLoop();
	});
}

/* -- EOF -- */