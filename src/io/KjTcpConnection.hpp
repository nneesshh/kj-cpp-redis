#pragma once

#include <functional>

#include "base/bip_buf.h"
#include "KjSimpleThreadIoContext.hpp"

class KjTcpConnection {
public:
	using READ_CB = std::function<void(KjTcpConnection&, bip_buf_t&)>;
	using CONNECT_CB = std::function<void(KjTcpConnection&, uint64_t)>;
	using DISCONNECT_CB = std::function<void(KjTcpConnection&, uint64_t)>;
	using ERROR_CB = std::function<void(KjTcpConnection&, kj::Exception&& exception)>;

	struct conn_attach_t {
		std::size_t _readSize;
		READ_CB _readCb;
		DISCONNECT_CB _disconnectCb;
		CONNECT_CB _connectCb;
	};

public:
	//! ctor & dtor
	KjTcpConnection(kj::Own<KjSimpleThreadIoContext> tioContext, uint64_t connid);
	~KjTcpConnection();

	//! copy ctor & assignment operator
	KjTcpConnection(const KjTcpConnection&) = delete;
	KjTcpConnection& operator=(const KjTcpConnection&) = delete;

	//! comparison operator
	bool operator==(const KjTcpConnection& rhs) const;
	bool operator!=(const KjTcpConnection& rhs) const;

	//! returns whether the connection is currently connected or not
	bool IsConnected() const {
		return _bIsConnected;
	}

	//! 
	kj::Promise<void> Connect(
		kj::StringPtr host,
		kj::uint port,
		CONNECT_CB connectCb,
		DISCONNECT_CB disconnectCb);

	//! 
	kj::Promise<void> Write(const void* buffer, size_t size);

	//! 
	void Disconnect();

	//! 
	kj::Promise<void> StartReadOp(const READ_CB& readCb);

	//!
	kj::Promise<void> DisconnectWatcher() {
		return _disconnectPromise.addBranch();
	}

	//! 
	kj::Promise<void> DelayReconnect();

private:
	//! operation
	kj::Promise<void> StartConnect();
	kj::Promise<void> Reconnect();
	kj::Promise<void> AsyncReadLoop();

	//!
	void OnDisconnect() {
		// clear bbuf
		bip_buf_reset(_bbuf);

		if (_connAttach._disconnectCb)
			_connAttach._disconnectCb(*this, _connid);
	}

public:
	kj::Own<KjSimpleThreadIoContext> _tioContext;
	kj::Own<kj::NetworkAddress> _addr;
	kj::Own<kj::AsyncIoStream> _stream;

	kj::ForkedPromise<void> _disconnectPromise = nullptr;
	kj::Own<kj::PromiseFulfiller<void>> _disconnectFulfiller;

private:
	uint64_t _connid = 0;
	kj::StringPtr _host;
	kj::uint _port = 0;

	conn_attach_t _connAttach;
	bip_buf_t *_bbuf = nullptr;

	bool _bIsConnected = false;
	bool _bIsReconnecting = false;
};

/*EOF*/