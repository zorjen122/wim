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

int StoreageNotifyAddFriend(Json::Value &request) {
  long from = request["from"].asInt64();
  long to = request["to"].asInt64();
  std::string requestMessage = request["requestMessage"].asString();
  db::FriendApply::Ptr friendApply(new db::FriendApply(
      from, to, db::FriendApply::Status::Wait, requestMessage));

  int ret = db::MysqlDao::GetInstance()->insertFriendApply(friendApply);
  return ret;
}

Json::Value NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;

  long from = request["from"].asInt64();
  long to = request["to"].asInt64();

  int ret = StoreageNotifyAddFriend(request);
  rsp["uid"] = Json::Value::Int64(to);

  auto toSession = OnlineUser::GetInstance()->GetUserSession(to);
  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  if (isLocalMachineOnline) {
    // 本地在线推送
    Json::Value senderRsp{};
    senderRsp["uid"] = Json::Value::Int64(from);
    senderRsp["requestMessage"] = request["requestMessage"];
    toSession->Send(request.toStyledString(), ID_NOTIFY_ADD_FRIEND_REQ);

    if (ret == -1) {
      LOG_INFO(businessLogger,
               "StoreageNotifyAddFriend save service failed, from-{}, to-{}",
               from, to);
      rsp["error"] = ErrorCodes::MysqlFailed;
      rsp["message"] = "数据库修改异常";
    } else {
      rsp["error"] = ErrorCodes::Success;
    }
    return rsp;
  }

  // 全局查找在线用户，所有设备中的在线用户都存放在redis中
  Json::Value userOnlineInfo =
      db::RedisDao::GetInstance()->getOnlineUserInfoObject(to);
  bool isOtherMachineOnline = !userOnlineInfo.empty();
  if (isOtherMachineOnline) {
    LOG_DEBUG(wim::businessLogger, "OfflineAddFriend to userinfo-{}",
              userOnlineInfo.toStyledString());

    // rpc转发
    auto machineId = userOnlineInfo["machineId"].asString();
    rpc::NotifyAddFriendRequest notifyRequest;
    rpc::NotifyAddFriendResponse notifyResponse;
    notifyRequest.set_from(from);
    notifyRequest.set_to(to);
    notifyRequest.set_requestmessage(request.toStyledString());

    // 通过MachineID路由到对应的机器，并转发
    LOG_INFO(wim::businessLogger,
             "forwardNotifyAddFriend(from: {}, to: {}) to machine: {}", from,
             to, machineId);
    notifyResponse =
        rpc::ImRpc::GetInstance()->getRpc(machineId)->forwardNotifyAddFriend(
            notifyRequest);

    if (notifyResponse.status() == "success") {
      rsp["error"] = ErrorCodes::Success;
    } else {
      rsp["message"] = "RPC转发异常";
      rsp["error"] = ErrorCodes::RPCFailed;
    }
    return rsp;
  } else {
    if (ret == -1) {
      LOG_INFO(businessLogger,
               "StoreageNotifyAddFriend save service failed, from-{}, to-{}",
               from, to);
      rsp["error"] = ErrorCodes::MysqlFailed;
      rsp["message"] = "数据库修改异常";
    } else {
      rsp["error"] = ErrorCodes::Success;
    }
    return rsp;
  }
}

Json::Value ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request) {
  Json::Value rsp;

  long from = request["from"].asInt64();
  long to = request["to"].asInt64();
  bool accept = request["accept"].asBool();
  std::string replyMessage = request["replyMessage"].asString();

  rsp["uid"] = Json::Value::Int64(to);

  long sessionKey = StoreageReplyAddFriend(request);
  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  if (isLocalMachineOnline) {
    Json::Value senderRsp{};
    senderRsp["uid"] = Json::Value::Int64(to);
    senderRsp["sessionKey"] = Json::Value::Int64(sessionKey);
    senderRsp["accept"] = accept;
    senderRsp["replyMessage"] = replyMessage;

    auto toSession = OnlineUser::GetInstance()->GetUserSession(to);
    toSession->Send(request.toStyledString(), ID_REPLY_ADD_FRIEND_REQ);

    if (sessionKey == -1) {
      LOG_ERROR(businessLogger,
                "StoreageReplyAddFriend is failed, from {}, to {}", from, to);
      rsp["error"] = ErrorCodes::MysqlFailed;
      rsp["message"] = "数据库修改发送异常";
    } else {
      rsp["error"] = ErrorCodes::Success;
    }
    return rsp;
  }

  Json::Value userInfo =
      db::RedisDao::GetInstance()->getOnlineUserInfoObject(to);

  bool isOtherMachineOnline = !userInfo.empty();
  if (isOtherMachineOnline) {
    LOG_INFO(businessLogger, "userinfo {}", userInfo.toStyledString());

    std::string machineId = userInfo["machineId"].asString();

    rpc::ReplyAddFriendRequest replyRequest;
    rpc::ReplyAddFriendResponse replyResponse;
    replyRequest.set_from(from);
    replyRequest.set_to(to);
    replyRequest.set_accept(accept);
    replyRequest.set_replymessage(replyMessage);
    replyResponse =
        rpc::ImRpc::GetInstance()->getRpc(machineId)->forwardReplyAddFriend(
            replyRequest);
    LOG_INFO(
        businessLogger,
        "forwardReplyAddFriend(from: {}, to: {}) to machine: {}, response: {}",
        from, to, machineId, replyResponse.status());

    if (replyResponse.status() == "success") {
      rsp["error"] = ErrorCodes::Success;
    } else {
      rsp["error"] = ErrorCodes::RPCFailed;
      rsp["message"] = "RPC转发异常";
    }
    return rsp;
  } else {
    if (sessionKey == -1) {
      LOG_ERROR(businessLogger,
                "StoreageReplyAddFriend is failed, from {}, to {}", from, to);
      rsp["error"] = ErrorCodes::MysqlFailed;
      rsp["message"] = "数据库修改发送异常";
    } else {
      rsp["error"] = ErrorCodes::Success;
    }
    return rsp;
  }
}

int StoreageReplyAddFriend(Json::Value &request) {

  long from = request["from"].asInt64();
  long to = request["to"].asInt64();
  bool accept = request["accept"].asBool();
  std::string replyMessage = request["replyMessage"].asString();

  int rt = -1;
  db::FriendApply::Status status{};
  std::string time = getCurrentDateTime();
  long sessionId{};
  if (accept) {
    LOG_INFO(businessLogger, "accept friend request, from {}, to {}", from, to);
    status = db::FriendApply::Status::Agree;
    long sessionId = db::RedisDao::GetInstance()->generateSessionId();
    db::Friend::Ptr friendData(new db::Friend(from, to, time, sessionId));

    rt = db::MysqlDao::GetInstance()->insertFriend(friendData);
    if (rt == -1) {
      LOG_ERROR(businessLogger, "insertFriend filed is failed, from {}, to {}",
                from, to);
      return -1;
    }
  } else {
    status = db::FriendApply::Status::Refuse;
  }

  db::FriendApply::Ptr friendApply(
      new db::FriendApply(from, to, status, replyMessage, time));
  rt = db::MysqlDao::GetInstance()->updateFriendApplyStatus(friendApply);
  if (rt == -1) {
    LOG_ERROR(businessLogger,
              "updateFriendApplyStatus filed is failed, from {}, to {}", from,
              to);
  }
  // 暂用
  return rt == -1 ? -1 : sessionId;
}
}; // namespace wim