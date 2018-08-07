#pragma once

#include <string>
#include <map>
#include <vector>

#include "platform_types.h"
#include "redis_service_def.h"
#include "IRedisService.h"

//------------------------------------------------------------------------------
/**
@class CRedisRankingProxy

(C) 2016 n.lee
*/

//------------------------------------------------------------------------------
/**
@brief CRedisRankingProxy
*/
class MY_REDIS_EXTERN CRedisRankingProxy {
public:
	CRedisRankingProxy(void *entry, const char *sMainId, const char *sSubid = "1");
	CRedisRankingProxy(const std::string& sModuleName, const char *sMainId, const char *sSubid = "1");
	virtual ~CRedisRankingProxy();

	using RESULT_PAIR_LIST = std::vector<CRedisReply>;
	using RESULT_LIST = std::vector<CRedisReply>;

	const std::string&			MainId() const {
		return _sMainId;
	}

	const std::string&			Subid() const {
		return _sSubid;
	}

	const std::string&			IdZSet() const {
		return _sIdZSet;
	}

	void						BindServiceEntry(void *entry) {
		_refEntry = entry;
	}

	void *						ServiceEntry() const {
		return _refEntry;
	}

	void						Commit();

	void						AddToZSet(double dScore, std::string& sMember);
	void						RemoveFromZSet(std::vector<std::string>& vMember);
	void						ZScore(std::string& sMember);

	void						Clear();

	void						Add(double dScore, std::string& sMember) {
		AddToZSet(dScore, sMember);
		Commit();
	}

	void						Remove(std::vector<std::string>& vMember) {
		RemoveFromZSet(vMember);
		Commit();
	}

	double						GetScore(std::string& sMember);

public:
	static const std::map<std::string, std::string>& MapScript();

private:
	void *_refEntry;

	std::string _sModuleName;
	std::string _sMainId;
	std::string _sSubid;
	std::string _sIdZSet;
	std::string _sIdHashOfCAS; // check and set

};

/*EOF*/