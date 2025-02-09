#pragma once

#include "Const.h"
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

class ChatSession;

// todo...
class UserBase {
private:
  size_t id;
  std::shared_ptr<ChatSession> network;
};

class User : public UserBase {
private:
  std::string name;
  std::string ip;
  std::string token;
  std::string sex;
  std::string icon;
};

static std::unordered_map<size_t, std::atomic<size_t>> seqUserMessage;

// ok
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
