#pragma once
//------------------------------------------------------------------------------
/**
	@class CKjRedisSubscriberWorkQueue

	(C) 2016 n.lee
*/
#include <memory>

#include "servercore/base/IServerCore.h"

#include "base/concurrent/readerwriterqueue.h"

#include "base/redis_service_def.h"
#include "base/IRedisService.h"

class CRedisSubscriber;

//------------------------------------------------------------------------------
/**
@brief CKjRedisSubscriberWorkQueue
*/
class CKjRedisSubscriberWorkQueue : public kj::TaskSet::ErrorHandler {
public:
	explicit CKjRedisSubscriberWorkQueue(
		CRedisSubscriber *pRedisHandle,
		redis_stub_param_t& param,
		const std::function<void(std::string&, std::string&)>& workCb1,
		const std::function<void(std::string&, std::string&, std::string&)>& workCb2);

	~CKjRedisSubscriberWorkQueue();
	
	using CallbackEntry = redis_cmd_pipepline_t;

	void						Run(svrcore_pipeworker_t *worker);

	bool						Add(redis_cmd_pipepline_t&& cmd);

	bool						IsDone() {
		return _done;
	}

	moodycamel::ReaderWriterQueue<CallbackEntry>& Callbacks() {
		return _callbacks;
	}

	void						Finish();

private:
	void						InitTasks();

public:
	static redis_cmd_pipepline_t	CreateCmdPipeline(
		int nSn,
		const std::string& sCommands,
		int nBuiltNum,
		redis_reply_cb_t&& reply_cb,
		dispose_cb_t&& dispose_cb) {

		redis_cmd_pipepline_t cp;
		cp._sn = nSn;
		cp._commands.append(sCommands);
		cp._built_num = nBuiltNum;
		cp._processed_num = 0;
		cp._reply_cb = std::move(reply_cb);
		cp._dispose_cb = std::move(dispose_cb);
		cp._state = redis_cmd_pipepline_t::QUEUEING;
		return cp;
	}

private:
	//! 
	void taskFailed(kj::Exception&& exception) override;

private:
	CRedisSubscriber *_refRedisHandle;
	redis_stub_param_t& _refParam;
	const std::function<void(std::string&, std::string&)>& _refWorkCb1;
	const std::function<void(std::string&, std::string&, std::string&)>& _refWorkCb2;

	volatile bool _done = false;
	volatile bool _finished = false;
	moodycamel::ReaderWriterQueue<CallbackEntry> _callbacks;

public:
	char _opCodeSend = 0;
	char _opCodeRecvBuf[1024];

	CallbackEntry _opCmd;

};
using CKjRedisSubscriberWorkQueuePtr = std::shared_ptr<CKjRedisSubscriberWorkQueue>;

/*EOF*/