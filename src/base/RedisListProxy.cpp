//------------------------------------------------------------------------------
//  RedisListProxy.cpp
//  (C) 2016 n.lee
//------------------------------------------------------------------------------
#include "RedisListProxy.h"

#include "redis_service_def.h"
#include "IRedisService.h"

int
static split(const char *str, int str_len, char **av, int av_max, char c) {
	int i, j;
	char *ptr = (char*)str;
	int count = 0;

	if (!str_len) str_len = strlen(ptr);

	for (i = 0, j = 0; i < str_len&& count < av_max; ++i) {
		if (ptr[i] != c)
			continue;
		else
			ptr[i] = 0x0;

		av[count++] = &(ptr[j]);
		j = i + 1;
		continue;
	}

	if (j < i) av[count++] = &(ptr[j]);
	return count;
}

static std::string s_sClear = "deb71530a8a2f6470a74c33cc9b3b3bba6fcc69e";
static std::string s_sPopAll = "b0b18b5d128a0d932e1d4e5543f11880cf0dfa8b";
static std::string s_sPopAllAndMark = "c9624702dd41daced1a4787ebaf4489dd5592874";
static std::string s_sLPushCAS = "967f59a855ba3b3a8b9ac5d9f8daa8457fbba45e";
static std::string s_sRPushCAS = "52e3cc9c03e402eb148548b1602bb82afe716424";

static std::string s_sTestMark = "9ca07b622605324eb92e9f01f5860a87fe56214d";
static std::string s_sSetCAS = "2eb401039d2bec89ac6e7311d60194384947b76a";
static std::string s_sResetCAS = "cd7ef4d8a95e631264688babc820a4e5a53b6ff1";

static std::string s_sIncrByIntCAS = "65d1f192c9e154865d50efba3a76dadceef93471";
static std::string s_sDecrByIntCAS = "efeaae5ef3b4a0d685383aff52aabee8d8571bc5";

static std::string s_sLootDirtyEntry = "2e644667554c78db35d5865af99df5b0505a1146";

static std::string s_sRestore = "8e04dd6a8e2fc14b6ac049cabd4197f02b293d9e";

//////////////////////////////////////////////////////////////////////////
static std::map<std::string, std::string> s_mapScript = {
	{ s_sClear,				"local r=redis.call('HGET',KEYS[1],'is_dirty');if (type(r)=='boolean' and not r or nil==r or ''==r)or(0==tonumber(r)) then redis.call('HDEL',KEYS[3],KEYS[2]);redis.call('DEL',KEYS[2]);redis.call('DEL',KEYS[1]);return true;else return false;end" },
	{ s_sPopAll,			"local r=redis.call('LRANGE',KEYS[2],0,-1);redis.call('DEL',KEYS[2]);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);return r" },
	{ s_sPopAllAndMark,		"local r=redis.call('LRANGE',KEYS[2],0,-1);redis.call('DEL',KEYS[2]);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);redis.call('HSET',KEYS[1],ARGV[1],ARGV[2]);redis.call('PUBLISH',KEYS[4],'0|'..ARGV[1]..':'..ARGV[2]);return r" },

	{ s_sLPushCAS,			"local r=redis.call('HGET',KEYS[1],ARGV[1]);if (type(r)=='boolean' and not r or nil==r or ''==r)or(r==ARGV[2]) then redis.call('HSET',KEYS[1],ARGV[1],ARGV[3]);redis.call('LPUSH',KEYS[2],ARGV[4]);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);local m=redis.call('LLEN',KEYS[2]);redis.call('PUBLISH',KEYS[4],m..'|'..ARGV[1]..':'..ARGV[3]);return true;else return false;end" },
	{ s_sRPushCAS,			"local r=redis.call('HGET',KEYS[1],ARGV[1]);if (type(r)=='boolean' and not r or nil==r or ''==r)or(r==ARGV[2]) then redis.call('HSET',KEYS[1],ARGV[1],ARGV[3]);redis.call('RPUSH',KEYS[2],ARGV[4]);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);local m=redis.call('LLEN',KEYS[2]);redis.call('PUBLISH',KEYS[4],m);return true;else return false;end" },

	{ s_sTestMark,			"local r=redis.call('HGET',KEYS[1],ARGV[1]);return r==ARGV[2]" },
	{ s_sSetCAS,			"local r=redis.call('HGET',KEYS[1],ARGV[1]);if (type(r)=='boolean' and not r or nil==r or ''==r)or(r==ARGV[2]) then redis.call('HSET',KEYS[1],ARGV[1],ARGV[3]);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);local m=redis.call('LLEN',KEYS[2]);redis.call('PUBLISH',KEYS[4],m);return true;else return false;end" },
	{ s_sResetCAS,			"local r=redis.call('HGET',KEYS[1],ARGV[1]);if r~=ARGV[2] then redis.call('HSET',KEYS[1],ARGV[1],ARGV[3]);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);local m=redis.call('LLEN',KEYS[2]);redis.call('PUBLISH',KEYS[4],m);return true;else return false;end" },

	{ s_sIncrByIntCAS,		"local r=redis.call('HGET',KEYS[1],ARGV[1]);r=(type(r)=='boolean' and not r or nil==r or ''==r)and(tonumber(ARGV[2]))or(tonumber(r)+tonumber(ARGV[2]));if r<=tonumber(ARGV[3]) then redis.call('HSET',KEYS[1],ARGV[1],r);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);local m=redis.call('LLEN',KEYS[2]);redis.call('PUBLISH',KEYS[4],m);return r;else return false;end" },
	{ s_sDecrByIntCAS,		"local r=redis.call('HGET',KEYS[1],ARGV[1]);r=(type(r)=='boolean' and not r or nil==r or ''==r)and(tonumber(ARGV[2]))or(tonumber(r)-tonumber(ARGV[2]));if r>=tonumber(ARGV[3]) then redis.call('HSET',KEYS[1],ARGV[1],r);redis.call('HSET',KEYS[1],'is_dirty',1);redis.call('HSET',KEYS[3],KEYS[2],KEYS[1]);local m=redis.call('LLEN',KEYS[2]);redis.call('PUBLISH',KEYS[4],m);return r;else return false;end" },

	{ s_sLootDirtyEntry,	"local a=redis.call('HGETALL',KEYS[1]);redis.call('DEL',KEYS[1]);local r={};local n=(a and #a) or 0;for i=1,n-1,2 do local l=a[i];local c=a[i+1];local d=redis.call('HGET',c,'is_dirty');redis.call('HDEL',c,'is_dirty');if 1==tonumer(d) then local t={};table.insert(t,l);table.insert(t,cmsgpack.pack(redis.call('LRANGE',l,0,-1)));table.insert(t,c);table.insert(t,cmsgpack.pack(redis.call('HGETALL',c)));table.insert(r,t);end;end;return r" },

	{ s_sRestore,			"local t=(ARGV[1] or '');local u=cmsgpack.unpack(t);local n=(u and #u) or 0;for i=1,n-1,2 do redis.call('HSET',KEYS[1],u[i],u[i+1]);end;t=(ARGV[2] or '');u=cmsgpack.unpack(t) or {};for _,v in ipairs(u) do redis.call('RPUSH',KEYS[2],v);end" },
};

//------------------------------------------------------------------------------
/**

*/
CRedisListProxy::CRedisListProxy(void *service_entry, const char *sMainId, const char *sSubid)
	: _refEntry(service_entry)
	, _sModuleName((static_cast<redis_service_entry_t *>(service_entry))->_sModuleName)
	, _sMainId(sMainId)
	, _sSubid(sSubid)
	, _sIdList(_sModuleName + ":" + _sMainId + ":" + sSubid + ":L")
	, _sIdHashOfCAS(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":CAS_H")
	, _sIdChanOfNotify(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":NTF_CHN") {

}

//------------------------------------------------------------------------------
/**

*/
CRedisListProxy::CRedisListProxy(const std::string& sModuleName, const char *sMainId, const char *sSubid)
	: _refEntry(nullptr)
	, _sModuleName(sModuleName)
	, _sMainId(sMainId)
	, _sSubid(sSubid)
	, _sIdList(_sModuleName + ":" + _sMainId + ":" + sSubid + ":L")
	, _sIdHashOfCAS(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":CAS_H")
	, _sIdChanOfNotify(_sModuleName + ":" + _sMainId + ":" + _sSubid + ":NTF_CHN") {
	
}

//------------------------------------------------------------------------------
/**

*/
CRedisListProxy::CRedisListProxy(const std::string& sIdList)
	: _refEntry(nullptr)
	, _sIdList(sIdList) {
	//
	SplitIdList(sIdList, _sModuleName, _sMainId, _sSubid);

	_sIdHashOfCAS = _sModuleName + ":" + _sMainId + ":" + _sSubid + ":CAS_H";
	_sIdChanOfNotify = _sModuleName + ":" + _sMainId + ":" + _sSubid + ":NTF_CHN";
}

//------------------------------------------------------------------------------
/**

*/
CRedisListProxy::~CRedisListProxy() {

}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::Commit() {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redis_reply_cb_t defaultCb = [](CRedisReply&) {};
	redisservice->Client().Commit(std::move(defaultCb));
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::LPushToList(std::string& sValue) {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().LPush(_sIdList.c_str(), std::vector<std::string>{ std::move(sValue) });
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::RPushToList(std::string& sValue) {
	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().RPush(_sIdList.c_str(), std::vector<std::string>{ std::move(sValue) });
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::Clear() {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sClear,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_null()) {
		// null means false
		fprintf(stderr, "[CRedisListProxy::Clear()] List(%s) is dirty, you must clean it first before -- Clear() --!!!!",
			_sIdList.c_str());
		system("pause");
		exit(-1);
	}
}

//------------------------------------------------------------------------------
/**

*/
const std::string
CRedisListProxy::LPop() {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().LPop(_sIdList.c_str());

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_string()) {
		//
		return std::move(reply.as_string());
	}
	return "";
}

//------------------------------------------------------------------------------
/**

*/
const std::string
CRedisListProxy::RPop() {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().RPop(_sIdList.c_str());

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_string()) {
		//
		return std::move(reply.as_string());
	}
	return "";
}

//------------------------------------------------------------------------------
/**

*/
int
CRedisListProxy::LLength() {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().LLen(_sIdList.c_str());

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return (int)reply.as_integer();
	}
	return 0;
}

//------------------------------------------------------------------------------
/**

*/
const std::string
CRedisListProxy::GetItemAt(int nIndex) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().LIndex(_sIdList.c_str(), nIndex);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_string()) {
		//
		return std::move(reply.as_string());
	}
	return "";
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::PopAll(RESULT_LIST& vOut) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sPopAll,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::PopAllAndMark(const std::string& sId, const std::string& sExpect, RESULT_LIST& vOut) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sPopAllAndMark,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, sExpect }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
}

//------------------------------------------------------------------------------
/**

*/
bool
CRedisListProxy::LPushCAS(const std::string& sId, const std::string& sExpect, const std::string& sDest, std::string& sValue) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sLPushCAS,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, sExpect, sDest, std::move(sValue) }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return (1 == reply.as_integer());
	}
	return false;
}

//------------------------------------------------------------------------------
/**

*/
bool
CRedisListProxy::RPushCAS(const std::string& sId, const std::string& sExpect, const std::string& sDest, std::string& sValue) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sRPushCAS,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, sExpect, sDest, std::move(sValue) }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return (1 == reply.as_integer());
	}
	return false;
}

//------------------------------------------------------------------------------
/**

*/
bool
CRedisListProxy::TestMark(const std::string& sId, const std::string& sExpect) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sTestMark,
		std::vector<std::string>{ _sIdHashOfCAS },
		std::vector<std::string>{ sId, sExpect }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return (1 == reply.as_integer());
	}
	return false;
}

//------------------------------------------------------------------------------
/**

*/
bool
CRedisListProxy::SetCAS(const std::string& sId, const std::string& sExpect, const std::string& sDest) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sSetCAS,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, sExpect, sDest }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return (1 == reply.as_integer());
	}
	return false;
}

//------------------------------------------------------------------------------
/**

*/
bool
CRedisListProxy::ResetCAS(const std::string& sId, std::string& sMustNotEqual, const std::string& sDest) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sResetCAS,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, std::move(sMustNotEqual), sDest }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return (1 == reply.as_integer());
	}
	return false;
}

//------------------------------------------------------------------------------
/**

*/
int
CRedisListProxy::IncrByIntCAS(const std::string& sId, int nIncrement, int nUpperBound) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sIncrByIntCAS,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, std::to_string(nIncrement), std::to_string(nUpperBound) }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return reply.as_integer();
	}
	return INT_MIN;
}

//------------------------------------------------------------------------------
/**

*/
int
CRedisListProxy::DecrByIntCAS(const std::string& sId, int nDecrement, int nLowerBound) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(_refEntry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sDecrByIntCAS,
		std::vector<std::string>{ _sIdHashOfCAS, _sIdList, entry->_listDirtyEntry, _sIdChanOfNotify },
		std::vector<std::string>{ sId, std::to_string(nDecrement), std::to_string(nLowerBound) }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//
		return reply.as_integer();
	}
	return INT_MAX;
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::SplitIdList(const std::string& sIdList, std::string& sOutModuleName, std::string& sOutMainId, std::string& sOutSubid) {
	std::string sTmp = sIdList;
	char* chArr[8] = { 0 };
	int n = split(sTmp.c_str(), sTmp.length(), chArr, 8, ':');
	if (n >= 3) {
		sOutModuleName = chArr[0];
		sOutMainId = chArr[1];
		sOutSubid = chArr[2];
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::LootDirtyEntry(void *service_entry, DIRTY_ENTRY_LIST& vOut) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(service_entry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sLootDirtyEntry,
		std::vector<std::string>{ entry->_listDirtyEntry },
		std::vector<std::string>{ }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_array()) {
		//
		vOut = std::move(reply.as_array());
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListProxy::Restore(void *service_entry, std::string& sIdList, std::string& sListVal, std::string& sIdHashOfCAS, std::string& sHashOfCASVal) {

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(service_entry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	redisservice->Client().EvalSha(
		s_sRestore,
		std::vector<std::string>{ sIdHashOfCAS, sIdList },
		std::vector<std::string>{ sHashOfCASVal, sListVal }
	);

	CRedisReply reply = redisservice->Client().BlockingCommit();
	if (reply.ok()
		&& reply.is_integer()) {
		//

	}
}

//------------------------------------------------------------------------------
/**

*/
const std::map<std::string, std::string>&
CRedisListProxy::MapScript() {
	return s_mapScript;
}

//////////////////////////////////////////////////////////////////////////
CRedisListSubject::MAP_OBSERVER_REG_LIST CRedisListSubject::s_mapObserverRegList;

//------------------------------------------------------------------------------
/**

*/
void
CRedisListSubject::OnGotChannelMessage(const std::string& chan, const std::string& msg) {

	MAP_OBSERVER_REG_LIST::iterator it = s_mapObserverRegList.find(chan);
	if (it != s_mapObserverRegList.end()) {
		// callback
		OBSERVER_REG_LIST& reg_list = it->second;
		for (auto& it2 : reg_list) {
			auto& reg = it2;
			if (!reg._is_deleted
				&& reg._cb) {
				reg._cb(chan, msg);
			}
		}

		// real delete
		OBSERVER_REG_LIST::iterator it3 = reg_list.begin(),
			itEnd3 = reg_list.end();
		while (it3 != itEnd3) {
			observer_reg_t& reg = (*it3);
			if (reg._is_deleted) {
				it3 = reg_list.erase(it3);
				itEnd3 = reg_list.end();
				continue;
			}

			//
			++it3;
		}
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListSubject::SetObservable(void *service_entry, bool bFlag) {

#define REG_NAME "redis_list_subject"

	redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(service_entry);
	IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
	if (bFlag) {
		redisservice->Subscriber().AddChannelMessageCb(REG_NAME, &CRedisListSubject::OnGotChannelMessage);
	}
	else {
		redisservice->Subscriber().RemoveChannelMessageCb(REG_NAME);
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListSubject::AttachObserver(const uintptr_t regid, const CRedisListProxy& observer, notify_cb_t& cb) {

	const std::string& sChanId = observer.IdChanOfNotify();

	OBSERVER_REG_LIST& reg_list = s_mapObserverRegList[sChanId];
	observer_reg_t *reg = nullptr;

	// check reg exist
	for (auto& it : reg_list) {
		if (regid == it._regid) {
			reg = &it;
			break;
		}
	}

	// not exist
	if (nullptr == reg) {
		// create reg
		reg_list.resize(reg_list.size() + 1);
		reg = &reg_list[reg_list.size() - 1];
		reg->_regid = regid;
		reg->_is_deleted = false;

		// assign cb
		reg->_cb = std::move(cb);

		// subscribe
		redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(observer.ServiceEntry());
		IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
		redisservice->Subscriber().Subscribe(sChanId);
	}
	else {
		// check deleted
		if (reg->_is_deleted) {
			//
			reg->_is_deleted = false;

			// subscribe
			redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(observer.ServiceEntry());
			IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
			redisservice->Subscriber().Subscribe(sChanId);
		}

		// re-assign cb
		reg->_cb = std::move(cb);
	}
}

//------------------------------------------------------------------------------
/**

*/
void
CRedisListSubject::DetachObserver(const uintptr_t regid, const CRedisListProxy& observer) {

	const std::string& sChanId = observer.IdChanOfNotify();

	MAP_OBSERVER_REG_LIST::iterator it = s_mapObserverRegList.find(sChanId);
	if (it != s_mapObserverRegList.end()) {

		OBSERVER_REG_LIST& reg_list = it->second;
		OBSERVER_REG_LIST::iterator it2 = reg_list.begin(),
			itEnd2 = reg_list.end();
		while (it2 != itEnd2) {

			observer_reg_t& reg = (*it2);
			if (regid == reg._regid) {
				//
				reg._is_deleted = true;

				// unsubscribe
				redis_service_entry_t *entry = static_cast<redis_service_entry_t *>(observer.ServiceEntry());
				IRedisService *redisservice = static_cast<IRedisService *>(entry->_redisservice);
				redisservice->Subscriber().Unsubscribe(sChanId);
				break;
			}

			//
			++it2;
		}
	}
}

/* -- EOF -- */