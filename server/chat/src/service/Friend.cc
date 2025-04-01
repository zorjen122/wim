#include "Friend.h"
#include "Channel.h"
#include "MysqlOperator.h"
#include "OnlineUser.h"
#include "Service.h"

#include <json/json.h>
#include <spdlog/spdlog.h>

Friend::Friend(int from, int to, std::string name, std::string icon) {
  channel = std::make_shared<Channel>(from, to, Channel::Type::FRIEND);
  this->name = name;
  this->icon = icon;
}

Friend::~Friend(){};

int OnlineAddFriend(int seq, int from, int to,
                    std::shared_ptr<ChatSession> toSession) {
  if (toSession == nullptr) {
    spdlog::error("[ServiceSystem::OnlineAddFriend] toSession is nullptr");
    return -1;
  }

  Json::Value rsp{};
  rsp["seq"] = seq;
  rsp["from"] = from;
  rsp["to"] = to;
  rsp["error"] = ErrorCodes::Success;

  // todo
  // MySqlOperator::GetInstance()->AddFriend(from, to);

  // 当客户端接收到ID_ADD_FRIEND_REQ时，意味着这只能是服务端的消息
  toSession->Send(rsp.toStyledString(), ID_ADD_FRIEND_REQ);
  OnRewriteTimer(toSession, seq, rsp.toStyledString(), ID_ADD_FRIEND_REQ, to);

  return 0;
}

int OfflineAddFriend(int seq, int from, int to, const std::string &msgData) {
  spdlog::info("[ServiceSystem::OfflineAddFriend] seq-{}, from-{}, to-{}", seq,
               from, to);
  bool rt = MysqlOperator::GetInstance()->SaveService(from, to, msgData);
  if (rt == false) {
    spdlog::error("OffineAddFriend save service to mysql failed");
    return -1;
  }

  return 0;
}

void AddFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_ADD_FRIEND_RSP);
  });

  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParser;
    spdlog::error("[ServiceSystem::AddFriend] parse msg_data error!");
    return;
  }

  int from = req["from"].asInt();
  int to = req["to"].asInt();

  // bool hasUser = MySqlOperator::GetInstance()->HasUser(to);
  bool hasUser = true;
  if (!hasUser) {
    rsp["uid"] = to;
    rsp["error"] = ErrorCodes::UserNotOnline;
    rsp["status"] = "x";
    spdlog::error("[ServiceSystem::AddFriend] no this a user, user-{}", to);
    return;
  }

  bool isOnline = OnlineUser::GetInstance()->isOnline(to);

  rsp["uid"] = to;
  rsp["error"] = ErrorCodes::Success;
  rsp["status"] = "wait";
  if (isOnline) {
    auto toSession = OnlineUser::GetInstance()->GetUser(to);
    int rt = OnlineAddFriend(util::seqGenerator, from, to, toSession);
    if (rt == -1) {
      rsp["error"] = -1;
      return;
    }
  } else {
    int rt = OfflineAddFriend(util::seqGenerator, from, to, msgData);
    if (rt == -1) {
      rsp["error"] = -1;
    }
  }

  OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                 ID_ADD_FRIEND_RSP, from);
  ++util::seqGenerator;
}

int OnlineRemoveFriend(int seq, int from, int to,
                       std::shared_ptr<ChatSession> toSession) {
  if (toSession == nullptr) {
    spdlog::error("[ServiceSystem::OnlineRemoveFriend] toSession is nullptr");
    return -1;
  }

  // todo... remove friend in mysql
  bool isRemoved = MysqlOperator::GetInstance()->RemovePair(from, to);

  if (isRemoved) {
    Json::Value notify{};
    notify["seq"] = seq;
    notify["from"] = from;
    notify["to"] = to;
    toSession->Send(notify.toStyledString(), ID_REMOVE_FRIEND_REQ);
    OnRewriteTimer(toSession, seq, notify.toStyledString(),
                   ID_REMOVE_FRIEND_REQ, to);
  }

  spdlog::error(
      "[ServiceSystem::OnlineRemoveFriend] mysql remove friend failed");
  return -1;
}

int OfflineRemoveFriend(int seq, int from, int to, const std::string &msgData) {
  spdlog::info("[ServiceSystem::OfflineRemoveFriend] seq-{}, from-{}, to-{}",
               seq, from, to);
  int rt = MysqlOperator::GetInstance()->SaveService(from, to, msgData);
  if (rt == false) {
    spdlog::error(
        "[ServiceSystem::OffineAddFriend] save service to mysql failed");
    return -1;
  }

  return 0;
}

void RemoveFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_ADD_FRIEND_RSP);
  });

  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParser;
    spdlog::error("[ServiceSystem::RemoveFriend] parse msg_data error!");
    return;
  }

  int from = req["from"].asInt();
  int to = req["to"].asInt();

  // todo... hasFriend function in mysql
  // bool hasFriend = MySqlOperator::GetInstance()->PairSearch(from, to);
  bool hasFriend = true;
  if (!hasFriend) {
    rsp["uid"] = to;
    rsp["error"] = ErrorCodes::UserNotFriend;
    rsp["status"] = "x";
    spdlog::error("[ServiceSystem::RemoveFriend] no this a user, user-{}", to);
    return;
  }

  bool isOnline = OnlineUser::GetInstance()->isOnline(to);

  rsp["uid"] = to;
  rsp["error"] = ErrorCodes::Success;
  rsp["status"] = "wait";
  if (isOnline) {
    auto toSession = OnlineUser::GetInstance()->GetUser(to);
    int rt = OnlineRemoveFriend(util::seqGenerator, from, to, toSession);
    if (rt == -1) {
      rsp["error"] = -1;
      return;
    }
  } else {
    int rt = OfflineRemoveFriend(util::seqGenerator, from, to, msgData);
    if (rt == -1) {
      rsp["error"] = -1;
    }
  }

  OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                 ID_ADD_FRIEND_RSP, from);
  ++util::seqGenerator;
}
