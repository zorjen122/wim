#include "OnlineUser.h"
#include "Mysql.h"
#include "Redis.h"

namespace util {

auto RandomUid() {
  // todo: mult-server random uid allocate
  return "";
}
} // namespace util

OnlineUser::~OnlineUser() { sessionMap.clear(); }

std::shared_ptr<ChatSession> OnlineUser::GetUser(size_t uid) {
  std::lock_guard<std::mutex> lock(sessionMutex);
  auto iter = sessionMap.find(uid);
  if (iter == sessionMap.end()) {
    // else(wim::db::RedisDao::GetInstance()->HasUser(uid))
    return nullptr;
  }

  return iter->second;
}

void OnlineUser::MapUser(size_t uid, std::shared_ptr<ChatSession> session) {
  std::lock_guard<std::mutex> lock(sessionMutex);
  // wim::db::RedisDao::GetInstance()->Set(PREFIX_REDIS_UIP + to_string(uid));
  sessionMap[uid] = session;
}

void OnlineUser::RemoveUser(size_t uid) {
  // wim::db::RedisDao::GetInstance()->Del(PREFIX_REDIS_UIP + to_string(uid));
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessionMap.erase(uid);
  }
}

OnlineUser::OnlineUser() {}

bool OnlineUser::isOnline(size_t uid) { return GetUser(uid) != nullptr; }