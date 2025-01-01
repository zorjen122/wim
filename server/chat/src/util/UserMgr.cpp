#include "UserManager.h"
#include "ChatSession.h"
#include "RedisManager.h"

UserManager::~UserManager()
{
	_sessionMap.clear();
}

std::shared_ptr<ChatSession> UserManager::GetSession(int uid)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	auto iter = _sessionMap.find(uid);
	if (iter == _sessionMap.end())
	{
		return nullptr;
	}

	return iter->second;
}

void UserManager::SetUserSession(int uid, std::shared_ptr<ChatSession> session)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	_sessionMap[uid] = session;
}

void UserManager::RemoveSession(int uid)
{
	auto uid_str = std::to_string(uid);
	// 因为再次登录可能是其他服务器，所以会造成本服务器删除key，其他服务器注册key的情况
	//  有可能其他服务登录，本服删除key造成找不到key的情况

	// RedisManager::GetInstance()->Del(PREFIX_REDIS_UIP + uid_str);

	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		_sessionMap.erase(uid);
	}
}

UserManager::UserManager()
{
}
