#pragma once

#include "Const.h"
#include <memory>

class ChatSession;
class OnlineUser : public Singleton<OnlineUser> {
  friend class Singleton<OnlineUser>;

public:
  ~OnlineUser();
  std::shared_ptr<ChatSession> GetUser(size_t uid);
  void MapUser(size_t uid, std::shared_ptr<ChatSession> session);
  void RemoveUser(size_t uid);
  bool isOnline(size_t uid);

private:
  OnlineUser();
  std::mutex sessionMutex;
  std::unordered_map<size_t, std::shared_ptr<ChatSession>> sessionMap;
};
