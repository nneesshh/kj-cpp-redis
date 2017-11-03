#pragma once

#include <functional>

#include "../base/bip_buf.h"
#include "KjSimpleThreadIoContext.h"

class KjTcpClient {
public:
	using READ_CB = std::function<void(KjTcpClient&, bip_buf_t&)>;
	using CONNECT_CB = std::function<void(KjTcpClient&, uint64_t)>;
	using DISCONNECT_CB = std::function<void(KjTcpClient&, uint64_t)>;

	struct conn_attach_t {
		std::size_t _readSize;
		READ_CB _readCb;
		DISCONNECT_CB _disconnectCb;
		CONNECT_CB _connectCb;
	};

public:
	//! ctor & dtor
	KjTcpClient(kj::Own<KjSimpleThreadIoContext> tioContext, size_t readSize);
	~KjTcpClient();

	//! copy ctor & assignment operator
	KjTcpClient(const KjTcpClient&) = delete;
	KjTcpClient& operator=(const KjTcpClient&) = delete;

	//! comparison operator
	bool operator==(const KjTcpClient& rhs) const;
	bool operator!=(const KjTcpClient& rhs) const;

	//! returns whether the client is currently connected or not
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
	kj::Promise<void> StartReadOp(READ_CB readCb);

	//! 
	kj::Promise<void> DelayReconnect() {
		OnDisconnect();
		return Reconnect();
	}

private:
	//! operation
	kj::Promise<void> StartConnect();
	kj::Promise<void> Reconnect();
	kj::Promise<void> AsyncReadLoop();

	//!
	void OnDisconnect() {

		if (IsConnected()) {
			_bIsConnected = false;

			// return reserved space
			bip_buf_commit(_bbuf, 0);

			if (_connAttach._disconnectCb)
				_connAttach._disconnectCb(*this, _connid);
		}
	}

public:
	kj::Own<KjSimpleThreadIoContext> _tioContext;

private:
	kj::Own<kj::NetworkAddress> _addr;
	kj::Own<kj::AsyncIoStream> _stream;
	uint64_t _connid = 0;

	conn_attach_t _connAttach;
	bip_buf_t *_bbuf = nullptr;

	bool _bIsConnected = false;
	bool _bIsReconnecting = false;
		
	kj::ForkedPromise<void> _disconnectPromise = nullptr;
	kj::Own<kj::PromiseFulfiller<void>> _disconnectFulfiller;
};

/*EOF*/