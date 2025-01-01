#pragma once
#include "Singleton.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class ChatSession;
class UserManager : public Singleton<UserManager>
{
	friend class Singleton<UserManager>;

public:
	~UserManager();
	std::shared_ptr<ChatSession> GetSession(int uid);
	void SetUserSession(int uid, std::shared_ptr<ChatSession> session);
	void RemoveSession(int uid);

private:
	UserManager();
	std::mutex _session_mtx;
	std::unordered_map<int, std::shared_ptr<ChatSession>> _sessionMap;
};
