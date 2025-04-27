#pragma once

#include "ChatSession.h"
#include "Const.h"
#include "DbGlobal.h"
#include <atomic>
#include <jsoncpp/json/value.h>
#include <memory>
#include <string>
#include <unordered_map>
namespace wim {

static std::unordered_map<long, std::atomic<long>> seqUserMessage;

// ok
class OnlineUser : public Singleton<OnlineUser> {
  friend class Singleton<OnlineUser>;

public:
  ~OnlineUser();
  ChatSession::Ptr GetUserSession(long uid);

  bool MapUser(db::UserInfo::Ptr userInfo, ChatSession::Ptr session);
  void RemoveUser(long uid);
  bool isOnline(long uid);

private:
  OnlineUser();
  std::mutex sessionMutex;
  std::unordered_map<long, ChatSession::Ptr> sessionMap;
};
}; // namespace wim