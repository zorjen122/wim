#include "OnlineUser.h"
#include "DbGlobal.h"
#include "Logger.h"
#include "Mysql.h"
#include "Redis.h"

namespace wim {

OnlineUser::~OnlineUser() { sessionMap.clear(); }
// userId:machineId
// machineId -> machine IP

ChatSession::Ptr OnlineUser::GetUserSession(long uid) {
  std::lock_guard<std::mutex> lock(sessionMutex);
  auto iter = sessionMap.find(uid);
  if (iter == sessionMap.end()) {
    return nullptr;
  }

  return iter->second;
}

bool OnlineUser::MapUser(db::UserInfo::Ptr userInfo, ChatSession::Ptr session) {
  YAML::Node config = Configer::getNode("server");
  std::string selfMachineId = config["self"]["name"].as<std::string>();

  bool status =
      db::RedisDao::GetInstance()->setOnlineUserInfo(userInfo, selfMachineId);
  if (status == false) {
    LOG_WARN(businessLogger, "setOnlineUserInfo failed, uid:{}, machineId:{}",
             userInfo->uid, selfMachineId);
    return false;
  }

  std::lock_guard<std::mutex> lock(sessionMutex);
  sessionMap[userInfo->uid] = session;
  return true;
}

void OnlineUser::RemoveUser(long uid) {
  // redis原子操作
  db::RedisDao::GetInstance()->delOnlineUserInfo(uid);

  std::lock_guard<std::mutex> lock(sessionMutex);
  sessionMap.erase(uid);
}

OnlineUser::OnlineUser() {}

bool OnlineUser::isOnline(long uid) { return GetUserSession(uid) != nullptr; }
}; // namespace wim