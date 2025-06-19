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
  UserInfo() = default;
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
  Friend() = default;
  Friend(size_t uidA, size_t uidB, std::string createTime, size_t sessionId)
      : uidA(uidA), uidB(uidB), createTime(std::move(createTime)),
        sessionId(sessionId) {}

  size_t uidA;
  size_t uidB;
  std::string createTime;

  // 更改为sessionId，消息服务器Id可在哈希中获取，此举是动态的
  size_t sessionId;
};

struct FriendApply {
  using Ptr = std::shared_ptr<FriendApply>;
  using FriendApplyGroup = std::shared_ptr<std::vector<Ptr>>;
  FriendApply() = default;
  enum Status {
    Wait = 0,
    Agree = 1,
    Refuse = 2,
  };

  FriendApply(size_t fromUid, size_t toUId, short status = 0,
              const std::string &content = "",
              const std::string &createTime = "")
      : from(fromUid), to(toUId), status(status), content(content),
        createTime(createTime) {}

  std::string formatStatus(short status) {
    switch (status) {
    case 0:
      return "wait";
    case 1:
      return "agree";
    case 2:
      return "refuse";
    default:
      return "unknown";
    }
  }
  size_t from;
  size_t to;
  short status;

  // 用于发起申请和回应时附带的消息内容
  std::string content;
  std::string createTime;
};

struct Message {
  using Ptr = std::shared_ptr<Message>;
  using MessageGroup = std::shared_ptr<std::vector<Message::Ptr>>;
  enum Type { TEXT = 1, IMAGE = 2, AUDIO = 3, VIDEO = 4, FILE = 5 };
  enum Status {
    WITHDRAW = 0,
    WAIT = 1,
    DONE = 2,
  };
  Message() = default;
  Message(long messageId, long fromUid, long toUid, std::string sessionKey,
          short type, std::string content, short status,
          std::string sendDateTime = "", std::string readDateTime = "")
      : messageId(messageId), from(fromUid), to(toUid), type(type),
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
  long messageId;
  long from;
  long to;
  std::string sessionKey;
  short type;
  std::string content;
  short status;
  std::string sendDateTime;
  std::string readDateTime;
};

// 新增，待定

struct GroupManager {
  using Ptr = std::shared_ptr<GroupManager>;
  GroupManager() = default;
  GroupManager(long gid, long sessionKey, std::string name,
               std::string createTime = "")
      : gid(gid), sessionKey(sessionKey), name(name), createTime(createTime) {}
  long gid;
  long sessionKey;
  std::string name;
  std::string createTime;
};

struct GroupMember {
  using Ptr = std::shared_ptr<GroupMember>;
  using MemberList = std::vector<GroupMember::Ptr>;

  enum Role { Member, Manager, Master };
  enum Speech { NORMAL, BAN };
  GroupMember() = default;
  GroupMember(long gid, long uid, short role, std::string joinTime = "",
              short speech = 0, std::string memberName = "")
      : gid(gid), uid(uid), role(role), joinTime(std::move(joinTime)),
        speech(speech), memberName(std::move(memberName)) {}
  long gid;
  long uid;
  short role;
  std::string joinTime;

  // other
  short speech;
  std::string memberName;
};

// 暂放
struct File {
  using Ptr = std::shared_ptr<File>;

  enum Type { IMAGE = 1, AUDIO = 2, VIDEO = 3, TEXT = 4 };
  File() = default;
  long seq;
  long offset;
  long total;
  Type type;

  std::string data;
  std::string name;
  std::string savePath;
  std::string createTime;
};

struct GroupApply {
  using Ptr = std::shared_ptr<GroupApply>;
  using GroupApplyList = std::shared_ptr<std::vector<GroupApply::Ptr>>;
  GroupApply(long requestor, long handler, long gid, short type, short status,
             std::string message, std::string updateTime)
      : requestor(requestor), handler(handler), gid(gid), type(type),
        status(status), message(message), updateTime(updateTime) {}

  // Type: 1.加群、2.退群、3.升权、4.降权、5、邀群、6.踢出
  enum Type {
    Add = 1,
    Delete = 2,
    Promote = 3,
    Demote = 4,
    Invite = 5,
    Kick = 6
  };

  enum Status { Wait = 0, Agree = 1, Refuse = 2 };

  long requestor;
  long handler;
  long gid;
  short type;
  short status;
  std::string message;
  std::string updateTime;
};

} // namespace wim::db