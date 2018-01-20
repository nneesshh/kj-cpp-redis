#pragma once

#include <string>
#include <map>
#include <vector>

#include "IRedisService.h"

//------------------------------------------------------------------------------
/**
@class CRedisListProxy

(C) 2016 n.lee
*/
#include "platform_types.h"

//------------------------------------------------------------------------------
/**
@brief CRedisListProxy
*/
class MY_REDIS_EXTERN CRedisListProxy {
public:
	CRedisListProxy(void *service_entry, const char *sListId, const char *sSubid = "A");
	CRedisListProxy(const std::string& sModuleName, const char *sListId, const char *sSubid = "A");
	CRedisListProxy(const std::string& sIdList);
	virtual ~CRedisListProxy();

	using RESULT_LIST = std::vector<CRedisReply>;
	using DIRTY_ENTRY_LIST = std::vector<CRedisReply>;

	const std::string&			MainId() const {
		return _sMainId;
	}

	const std::string&			Subid() const {
		return _sSubid;
	}

	const std::string&			IdList() const {
		return _sIdList;
	}

	const std::string&			IdChanOfNotify() const {
		return _sIdChanOfNotify;
	}

	void						BindServiceEntry(void *service_entry) {
		_refEntry = service_entry;
	}

	void *						ServiceEntry() const {
		return _refEntry;
	}

	void						Commit();
	void						LPushToList(std::string& sValue);
	void						RPushToList(std::string& sValue);
	void						Clear();

	void						LPush(std::string& sValue) {
		LPushToList(std::move(sValue));
		Commit();
	}

	void						RPush(std::string& sValue) {
		RPushToList(std::move(sValue));
		Commit();
	}

	const std::string			LPop();
	const std::string			RPop();

	int							LLength();
	const std::string			GetItemAt(int nIndex);

	void						PopAll(RESULT_LIST& vOut);
	void						PopAllAndMark(const std::string& sId, const std::string& sExpect, RESULT_LIST& vOut);

	bool						LPushCAS(const std::string& sId, const std::string& sExpect, const std::string& sDest, std::string& sValue);
	bool						RPushCAS(const std::string& sId, const std::string& sExpect, const std::string& sDest, std::string& sValue);

	bool						TestMark(const std::string& sId, const std::string& sExpect);

	bool						SetCAS(const std::string& sId, const std::string& sExpect, const std::string& sDest);
	bool						ResetCAS(const std::string& sId, std::string& sMustNotEqual, const std::string& sDest);

	int							IncrByIntCAS(const std::string& sId, int nIncrement, int nUpperBound);
	int							DecrByIntCAS(const std::string& sId, int nDecrement, int nLowerBound);

public:
	static void					SplitIdList(const std::string& sIdList, std::string& sOutModuleName, std::string& sOutMainId, std::string& sOutSubid);
	static void					LootDirtyEntry(void *service_entry, DIRTY_ENTRY_LIST& vOut);

	static void					Restore(void *service_entry, std::string& sIdList, std::string& sListVal, std::string& sIdHashOfCAS, std::string& sHashOfCASVal);

	static const std::map<std::string, std::string>& MapScript();

private:
	void *_refEntry;

	std::string _sModuleName;
	std::string _sMainId;
	std::string _sSubid;
	std::string _sIdList;
	std::string _sIdHashOfCAS; // check and set
	std::string _sIdChanOfNotify; // pub sub notify

};

//------------------------------------------------------------------------------
/**
@brief CRedisListSubject
*/
class MY_REDIS_EXTERN CRedisListSubject {
public:
	using notify_cb_t = std::function<void(const std::string&, const std::string&)>;

	struct observer_reg_t {
		uintptr_t _regid;
		notify_cb_t _cb;

		bool _is_deleted;
	};
	using OBSERVER_REG_LIST = std::vector<observer_reg_t>;
	using MAP_OBSERVER_REG_LIST = std::map<std::string, OBSERVER_REG_LIST>;

	static void					OnGotChannelMessage(const std::string& chan, const std::string& msg);

	static void					SetObservable(void *service_entry, bool bFlag);

	static void					AttachObserver(const uintptr_t regid, const CRedisListProxy& observer, notify_cb_t& cb);
	static void					DetachObserver(const uintptr_t regid, const CRedisListProxy& observer);

	static MAP_OBSERVER_REG_LIST s_mapObserverRegList; // channel id 2 observer reg list
};

/*EOF*/