#pragma once

#include "Logger.h"
#include <memory>
#include <string>
#include <vector>

namespace wim::db {
struct User {
  using Ptr = std::shared_ptr<User>;
  User() = default;
  User(size_t id, size_t uid, std::string username, std::string password,
       std::string email, std::string createTime = "")
      : id(std::move(id)), uid(std::move(uid)), username(std::move(username)),
        password(std::move(password)), email(std::move(email)),
        createTime(std::move(createTime)) {}
  size_t id;
  size_t uid;
  std::string username;
  std::string password;
  std::string email;
  std::string createTime;
};

struct UserInfo {
  using Ptr = std::shared_ptr<UserInfo>;

  UserInfo(size_t uid, std::string name, short age, std::string sex,
           std::string headImageURL)
      : uid(std::move(uid)), name(std::move(name)), age(std::move(age)),
        sex(std::move(sex)), headImageURL(std::move(headImageURL)) {}

  size_t uid;
  std::string name;
  short age;
  std::string sex;
  std::string headImageURL;
};
struct Friend {
  using Ptr = std::shared_ptr<Friend>;
  using FriendGroup = std::shared_ptr<std::vector<Friend::Ptr>>;
  Friend(size_t uidA, size_t uidB, std::string createDateTime, size_t machineId)
      : uidA(uidA), uidB(uidB), createDateTime(std::move(createDateTime)),
        machineId(machineId) {}

  static std::string formatMachineId(size_t id) {
    switch (id) {
    case 1:
      return "hunan";
    case 2:
      return "beijing";
    case 3:
      return "shanghai";
    default:
      return "";
    }
  }
  size_t uidA;
  size_t uidB;
  std::string createDateTime;
  size_t machineId;
};

struct FriendApply {
  using Ptr = std::shared_ptr<FriendApply>;
  using FriendApplyGroup = std::shared_ptr<std::vector<FriendApply::Ptr>>;
  FriendApply(size_t fromUid, size_t toUId, short status = 0)
      : fromUid(fromUid), toUid(toUId) {}

  std::string formatStatus(short status) {
    return status == 0 ? "wait" : "done";
  }
  size_t fromUid;
  size_t toUid;
  short status;
};

struct Message {
  using Ptr = std::shared_ptr<Message>;
  using MessageGroup = std::shared_ptr<std::vector<Message::Ptr>>;
  Message(int messageId, int fromUid, int toUid, std::string sessionKey,
          short type, std::string content, short status,
          std::string sendDateTime, std::string readDateTime = "")
      : messageId(messageId), fromUid(fromUid), toUid(toUid), type(type),
        content(std::move(content)), status(status),
        sendDateTime(std::move(sendDateTime)),
        readDateTime(std::move(readDateTime)) {}

  std::string formatType(short type) {
    switch (type) {
    case 1:
      return "text";
    case 2:
      return "image";
    case 3:
      return "audio";
    case 4:
      return "video";
    case 5:
      return "file";
    default:
      LOG_DEBUG(dbLogger, "no such type value: {}", type);
      return "";
    }
  }
  std::string formatStatus(short status) {
    switch (status) {
    case 0:
      return "withdraw"; // 撤回
    case 1:
      return "wait";
    case 2:
      return "done";
    default:
      LOG_DEBUG(dbLogger, "no such status value: {}", status);
      return "";
    }
  }
  int messageId;
  int fromUid;
  int toUid;
  std::string sessionKey;
  short type;
  std::string content;
  short status;
  std::string sendDateTime;
  std::string readDateTime;
};

} // namespace wim::db