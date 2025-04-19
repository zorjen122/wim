#include "Friend.h"
#include "ImRpc.h"

#include "Const.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"
#include "Service.h"

#include "json/reader.h"
#include "json/value.h"
#include <json/json.h>
#include <spdlog/spdlog.h>
namespace wim {

int OnlineNotifyAddFriend(std::shared_ptr<ChatSession> user,
                          const Json::Value &request) {
  user->Send(request.toStyledString(), ID_NOTIFY_ADD_FRIEND_REQ);
  // on rewrite...
  return 0;
}

int OfflineAddFriend(int seq, int from, int to, const Json::Value &request) {
  spdlog::info("[Service::OfflineAddFriend] seq-{}, from-{}, to-{}", seq, from,
               to);
  // bool rt = wim::db::MysqlDao::GetInstance()->SaveService(from, to, request);
  bool rt = true; // todo...
  if (rt == false) {
    spdlog::error("OffineAddFriend save service to mysql failed");
    return -1;
  }

  return 0;
}

void SerachUser(std::shared_ptr<ChatSession> session, unsigned int msgID,
                const Json::Value &request) {
  Json::Value rsp;
  Defer defer([&] {
    auto rt = rsp.toStyledString();
    session->Send(rt, ID_SEARCH_USER_RSP);
  });
  auto username = request["username"].asString();
  auto user = db::MysqlDao::GetInstance()->getUser(username);
  if (user == nullptr) {
    rsp["error"] = -1;
    return;
  }
  auto userInfo = db::MysqlDao::GetInstance()->getUserInfo(user->uid);
  if (userInfo == nullptr) {
    rsp["error"] = -1;
    return;
  }
  rsp["uid"] = Json::Value::Int64(userInfo->uid);
  rsp["username"] = user->username;
  rsp["age"] = userInfo->age;
  rsp["headImageURL"] = userInfo->headImageURL;
  rsp["error"] = 0;
}
void NotifyAddFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                     const Json::Value &request) {
  Json::Value rsp;
  Defer defer([&] {
    auto rt = rsp.toStyledString();
    session->Send(rt, ID_NOTIFY_ADD_FRIEND_RSP);
  });

  long fromUid = request["fromUid"].asInt64();
  long toUid = request["toUid"].asInt64();

  auto user = OnlineUser::GetInstance()->GetUser(toUid);
  if (user != nullptr) {
    // 本地在线推送
    int status = OnlineNotifyAddFriend(user, request);
    if (status != 0) {
      LOG_DEBUG(wim::businessLogger,
                "OnlineAddFriend failed, uid-{}, status-{}", toUid, status);
    }
    LOG_DEBUG(wim::businessLogger, "OnlineAddFriend success, to-{}", toUid);
    rsp["error"] = 0;
    rsp["status"] = "wait";
  } else {
    // rpc转发服务
    auto source = db::RedisDao::GetInstance()->getUserId(toUid);
    Json::Reader parser;
    Json::Value userOnlineInfo;
    if (parser.parse(source, userOnlineInfo) == false) {
      rsp["error"] = ErrorCodes::JsonParser;
      spdlog::error("[Service::NotifyAddFriend] parse userOnlineInfo error!");
      return;
    }
    auto machineId = userOnlineInfo["machineId"].asString();
    rpc::NotifyAddFriendRequest notifyRequest;
    rpc::NotifyAddFriendResponse notifyResponse;
    notifyRequest.set_fromuid(fromUid);
    notifyRequest.set_touid(toUid);
    notifyRequest.set_requestmessage(request.toStyledString());
    notifyResponse =
        rpc::ImRpc::GetInstance()->getRpc(machineId)->forwardNotifyAddFriend(
            notifyRequest);
    if (notifyResponse.status() == "success") {
      rsp["error"] = 0;
      rsp["status"] = "wait";
    } else {
      rsp["error"] = -1;
    }
  }
}

int OnlineRemoveFriend(int seq, int from, int to,
                       std::shared_ptr<ChatSession> toSession) {
  if (toSession == nullptr) {
    spdlog::error("[Service::OnlineRemoveFriend] toSession is nullptr");
    return -1;
  }

  // todo... remove friend in mysql
  // bool isRemoved = wim::db::MysqlDao::GetInstance()->RemovePair(from, to);
  bool isRemoved = true;

  if (isRemoved) {
    Json::Value notify{};
    notify["seq"] = seq;
    notify["from"] = from;
    notify["to"] = to;
    toSession->Send(notify.toStyledString(), ID_REMOVE_FRIEND_REQ);
    OnRewriteTimer(toSession, seq, notify.toStyledString(),
                   ID_REMOVE_FRIEND_REQ, to);
  }

  spdlog::error("[Service::OnlineRemoveFriend] mysql remove friend failed");
  return -1;
}

int OfflineRemoveFriend(int seq, int from, int to, const Json::Value &request) {
  spdlog::info("[Service::OfflineRemoveFriend] seq-{}, from-{}, to-{}", seq,
               from, to);
  // int rt = wim::db::MysqlDao::GetInstance()->SaveService(from, to, request);
  bool rt = true; // todo...
  if (rt == false) {
    spdlog::error("[Service::OffineAddFriend] save service to mysql failed");
    return -1;
  }

  return 0;
}

void RemoveFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const Json::Value &request) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_NOTIFY_ADD_FRIEND_RSP);
  });

  int from = req["from"].asInt();
  int to = req["to"].asInt();

  // todo... hasFriend function in mysql
  // bool hasFriend = wim::db::MysqlDao::GetInstance()->PairSearch(from, to);
  bool hasFriend = true;
  if (!hasFriend) {
    rsp["uid"] = to;
    rsp["error"] = ErrorCodes::UserNotFriend;
    rsp["status"] = "x";
    spdlog::error("[Service::RemoveFriend] no this a user, user-{}", to);
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
    int rt = OfflineRemoveFriend(util::seqGenerator, from, to, request);
    if (rt == -1) {
      rsp["error"] = -1;
    }
  }

  OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                 ID_NOTIFY_ADD_FRIEND_RSP, from);
  ++util::seqGenerator;
}
}; // namespace wim