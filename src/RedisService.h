#pragma once
//------------------------------------------------------------------------------
/**
@class CRedisService

(C) 2016 n.lee
*/
#include "base/redis_service_def.h"
#include "base/IRedisService.h"

//------------------------------------------------------------------------------
/**
@brief CRedisService
*/
class CRedisService : public IRedisService {
public:
	CRedisService(redis_stub_param_t *param);
	virtual ~CRedisService();

public:
	virtual void				RunOnce() override {
		_redisClient->RunOnce();
		_redisSubscriber->RunOnce();
	}

	virtual IRedisClient&		Client() override {
		return *_redisClient;
	}

	virtual IRedisSubscriber&	Subscriber() override {
		return *_redisSubscriber;
	}

	virtual void				Shutdown() override;

public:
	static void					Send(const std::vector<std::string>& vPiece, std::string& sOutSingleCommand, std::string& sOutAllCommands, int& nOutBuiltNum) {
		sOutAllCommands.append(BuildCommand(vPiece, sOutSingleCommand));
		++nOutBuiltNum;
	}

	static std::string&			BuildCommand(const std::vector<std::string>& vPiece, std::string& sOutSingleCommand) {
		sOutSingleCommand.resize(0);
		sOutSingleCommand.append("*").append(std::to_string(vPiece.size())).append("\r\n");

		for (const auto& piece : vPiece) {
			sOutSingleCommand.append("$").append(std::to_string(piece.length())).append("\r\n").append(piece).append("\r\n");
		}
		return sOutSingleCommand;
	}

private:
	redis_stub_param_t _param;
	bool _bShutdown = false;

	IRedisClient *_redisClient;
	IRedisSubscriber *_redisSubscriber;

};

/*EOF*/