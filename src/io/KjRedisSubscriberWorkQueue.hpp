#pragma once
//------------------------------------------------------------------------------
/**
@class CKjRedisSubscriberWorkQueue

(C) 2016 n.lee
*/
#include <deque>
#include <mutex>
#include <thread>

#include "base/concurrent/readerwriterqueue.h"
#include "base/redis_service_def.h"
#include "base/IRedisService.h"

#include "io/KjSimpleIoContext.hpp"
#include "io/KjSimpleThreadIoContext.hpp"

//------------------------------------------------------------------------------
/**
@brief CKjRedisSubscriberWorkQueue
*/
class CKjRedisSubscriberWorkQueue : public kj::TaskSet::ErrorHandler {
public:
	explicit CKjRedisSubscriberWorkQueue(
		kj::Own<KjSimpleIoContext> rootContext,
		redis_stub_param_t& param,
		const std::function<void(std::string&, std::string&)>& workCb1,
		const std::function<void(std::string&, std::string&, std::string&)>& workCb2);

	~CKjRedisSubscriberWorkQueue();

	using dispose_cb_t = std::function<void()>;

	struct cmd_pipepline_t {
		enum CMD_PIPELINE_STATE {
			CMD_PIPELINE_STATE_QUEUEING = 1,
			CMD_PIPELINE_STATE_SENDING = 2,
			CMD_PIPELINE_STATE_COMMITTING = 3,
			CMD_PIPELINE_STATE_PROCESSING = 4,
			CMD_PIPELINE_STATE_PROCESS_OVER = 5,
		};

		int _sn;
		std::string _commands;
		int _built_num;
		int _processed_num;
		redis_reply_cb_t _reply_cb;
		dispose_cb_t _dispose_cb;
		CMD_PIPELINE_STATE _state;
	};
	using CallbackEntry = cmd_pipepline_t;

	bool						Add(cmd_pipepline_t&& cmd);

	bool						IsDone() {
		return _done;
	}

	moodycamel::ReaderWriterQueue<CallbackEntry>& Callbacks() {
		return _callbacks;
	}

	void						Finish();

private:
	void						Start();
	void						Run(kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope);

public:
	static cmd_pipepline_t		CreateCmdPipeline(
		int nSn,
		const std::string& sAllCommands,
		int nBuiltNum,
		redis_reply_cb_t&& reply_cb,
		dispose_cb_t&& dispose_cb) {

		cmd_pipepline_t cp;
		cp._sn = nSn;
		cp._commands.append(sAllCommands);
		cp._built_num = nBuiltNum;
		cp._processed_num = 0;
		cp._reply_cb = std::move(reply_cb);
		cp._dispose_cb = std::move(dispose_cb);
		cp._state = cmd_pipepline_t::CMD_PIPELINE_STATE_QUEUEING;
		return cp;
	}

private:
	//! 
	void taskFailed(kj::Exception&& exception) override;

private:
	kj::Own<KjSimpleIoContext> _rootContext;
	redis_stub_param_t& _refParam;
	const std::function<void(std::string&, std::string&)>& _refWorkCb1;
	const std::function<void(std::string&, std::string&, std::string&)>& _refWorkCb2;

	bool _done = false;
	moodycamel::ReaderWriterQueue<CallbackEntry> _callbacks;

	//! threads
	kj::AsyncIoProvider::PipeThread _pipeThread;

public:
	char _opCodeSend = 0;
	char _opCodeRecvBuf[1024];

	CallbackEntry _opCmd;

};
using CKjRedisSubscriberWorkQueuePtr = std::shared_ptr<CKjRedisSubscriberWorkQueue>;

/*EOF*/