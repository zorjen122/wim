#include "Friend.h"
#include "DbGlobal.h"
#include "ImRpc.h"

#include "Const.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <spdlog/spdlog.h>
#include <string>
namespace wim {

int OnlineNotifyAddFriend(ChatSession::Ptr user, Json::Value &request) {
  // on rewrite...
  user->Send(request.toStyledString(), ID_NOTIFY_ADD_FRIEND_REQ);

  // 暂行，保险方案
  return OfflineNotifyAddFriend(request);
}

int OfflineNotifyAddFriend(Json::Value &request) {
  long from = request["fromUid"].asInt64();
  long to = request["toUid"].asInt64();
  std::string requestMessage = request["requestMessage"].asString();
  db::FriendApply::Ptr friendApply(new db::FriendApply(
      from, to, db::FriendApply::Status::Wait, requestMessage));

  int ret = db::MysqlDao::GetInstance()->insertFriendApply(friendApply);
  return ret;
}

Json::Value NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;

  long fromUid = request["fromUid"].asInt64();
  long toUid = request["toUid"].asInt64();

  auto toSession = OnlineUser::GetInstance()->GetUserSession(toUid);
  bool isMachineOnline = OnlineUser::GetInstance()->isOnline(toUid);
  if (isMachineOnline) {
    // 本地在线推送
    int rt = OnlineNotifyAddFriend(toSession, request);
    if (rt == -1) {
      rsp["error"] = -1;
      rsp["message"] = "通知发送失败";
      return rsp;
    }

    LOG_DEBUG(wim::businessLogger, "OnlineAddFriend success, to-{}", toUid);
    rsp["error"] = 0;
    rsp["status"] = "wait";
    return rsp;
  }

  // 全局查找在线用户，所有设备中的在线用户都存放在redis中
  Json::Value userOnlineInfo =
      db::RedisDao::GetInstance()->getOnlineUserInfoObject(toUid);
  bool isOtherMachineOnline = !userOnlineInfo.empty();
  if (isOtherMachineOnline) {
    LOG_DEBUG(wim::businessLogger, "OfflineAddFriend toUId userinfo-{}",
              userOnlineInfo.toStyledString());

    // rpc转发
    auto machineId = userOnlineInfo["machineId"].asString();
    rpc::NotifyAddFriendRequest notifyRequest;
    rpc::NotifyAddFriendResponse notifyResponse;
    notifyRequest.set_fromuid(fromUid);
    notifyRequest.set_touid(toUid);
    notifyRequest.set_requestmessage(request.toStyledString());

    // 通过MachineID路由到对应的机器，并转发
    LOG_INFO(wim::businessLogger,
             "forwardNotifyAddFriend(from: {}, to: {}) to machine: {}", fromUid,
             toUid, machineId);
    notifyResponse =
        rpc::ImRpc::GetInstance()->getRpc(machineId)->forwardNotifyAddFriend(
            notifyRequest);
    if (notifyResponse.status() == "success") {
      rsp["error"] = ErrorCodes::Success;
      rsp["status"] = "wait";
    } else {
      rsp["error"] = -1;
    }
    return rsp;
  } else {
    int ret = OfflineNotifyAddFriend(request);
    if (ret != 0) {
      LOG_INFO(businessLogger,
               "OfflineAddFriend save service failed, from-{}, to-{}", fromUid,
               toUid);
      rsp["error"] = -1;
    } else {
      rsp["status"] = "wait";
    }
    return rsp;
  }
}

Json::Value ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request) {
  Json::Value rsp;

  long fromUid = request["fromUid"].asInt64();
  long toUid = request["toUid"].asInt64();
  bool accept = request["accept"].asBool();
  std::string replyMessage = request["replyMessage"].asString();

  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(toUid);
  if (isLocalMachineOnline) {
    auto toSession = OnlineUser::GetInstance()->GetUserSession(toUid);
    int rt = OnlineReplyAddFriend(toSession, request);

    if (rt == -1) {
      LOG_ERROR(
          businessLogger,
          "updateFriendApplyStatus update filed is failed, from {}, to {}",
          fromUid, toUid);
      rsp["error"] = -2;
      rsp["message"] = "数据库修改发送异常";
    } else {
      rsp["error"] = ErrorCodes::Success;
    }
    return rsp;
  }

  Json::Value userInfo =
      db::RedisDao::GetInstance()->getOnlineUserInfoObject(toUid);

  bool isOtherMachineOnline = !userInfo.empty();
  if (isOtherMachineOnline) {
    LOG_INFO(businessLogger, "userinfo {}", userInfo.toStyledString());

    std::string machineId = userInfo["machineId"].asString();

    rpc::ReplyAddFriendRequest replyRequest;
    rpc::ReplyAddFriendResponse replyResponse;
    replyRequest.set_fromuid(fromUid);
    replyRequest.set_touid(toUid);
    replyRequest.set_accept(accept);
    replyRequest.set_replymessage(replyMessage);
    replyResponse =
        rpc::ImRpc::GetInstance()->getRpc(machineId)->forwardReplyAddFriend(
            replyRequest);
    LOG_INFO(
        businessLogger,
        "forwardReplyAddFriend(from: {}, to: {}) to machine: {}, response: {}",
        fromUid, toUid, machineId, replyResponse.status());
    rsp["error"] = ErrorCodes::Success;

    return rsp;
  } else {
    int rt = OfflineReplyAddFriend(request);
    if (rt == -1) {
      LOG_ERROR(
          businessLogger,
          "updateFriendApplyStatus update filed is failed, from {}, to {}",
          fromUid, toUid);
      rsp["error"] = -2;
      rsp["message"] = "数据库修改发送异常";
    } else {
      rsp["error"] = ErrorCodes::Success;
    }
    return rsp;
  }
}
int OnlineReplyAddFriend(ChatSession::Ptr user, Json::Value &request) {

  // 暂行，保险方案
  int sessionId = OfflineReplyAddFriend(request);
  if (sessionId > 0) {
    request["sessionId"] = sessionId;
    user->Send(request.toStyledString(), ID_REPLY_ADD_FRIEND_REQ);

    return 0;
  }

  return sessionId;
}

int OfflineReplyAddFriend(Json::Value &request) {

  long fromUid = request["fromUid"].asInt64();
  long toUid = request["toUid"].asInt64();
  bool accept = request["accept"].asBool();
  std::string replyMessage = request["replyMessage"].asString();

  int rt = -1;
  db::FriendApply::Status status{};
  std::string time = getCurrentDateTime();
  long sessionId{};
  if (accept) {
    status = db::FriendApply::Status::Agree;
    long sessionId = db::RedisDao::GetInstance()->generateSessionId();
    db::Friend::Ptr friendData(new db::Friend(fromUid, toUid, time, sessionId));

    rt = db::MysqlDao::GetInstance()->insertFriend(friendData);
    if (rt == -1) {
      LOG_ERROR(businessLogger, "insertFriend filed is failed, from {}, to {}",
                fromUid, toUid);
      return -1;
    }
  } else {
    status = db::FriendApply::Status::Refuse;
  }

  db::FriendApply::Ptr friendApply(
      new db::FriendApply(fromUid, toUid, status, replyMessage, time));
  rt = db::MysqlDao::GetInstance()->updateFriendApplyStatus(friendApply);
  if (rt == -1) {
    LOG_ERROR(businessLogger,
              "updateFriendApplyStatus filed is failed, from {}, to {}",
              fromUid, toUid);
  }
  // 暂用
  return rt == -1 ? -1 : sessionId;
}

}; // namespace wim