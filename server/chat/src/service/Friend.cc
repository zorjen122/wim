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
  bool skipStorage = request.get("__skipStorage", false).asBool();

  int ret = skipStorage ? 0 : StoreageNotifyAddFriend(request);
  rsp["uid"] = Json::Value::Int64(to);
  if (ret == -1) {
    LOG_INFO(businessLogger,
             "StoreageNotifyAddFriend save service failed, from-{}, to-{}",
             from, to);
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库修改异常";
    return rsp;
  }

  auto toSession = OnlineUser::GetInstance()->GetUserSession(to);
  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  if (isLocalMachineOnline) {
    // 本地在线推送
    Json::Value senderRsp = request;
    senderRsp.removeMember("__skipStorage");
    toSession->Send(senderRsp.toStyledString(), ID_NOTIFY_ADD_FRIEND_REQ);

    rsp["error"] = ErrorCodes::Success;
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
    notifyRequest.set_requestmessage(request["requestMessage"].asString());

    // 通过MachineID路由到对应的机器，并转发
    LOG_INFO(wim::businessLogger,
             "forwardNotifyAddFriend(from: {}, to: {}) to machine: {}", from,
             to, machineId);
    auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
    if (rpcNode == nullptr) {
      rsp["message"] = "RPC目标机器不存在";
      rsp["error"] = ErrorCodes::RPCFailed;
      return rsp;
    }
    notifyResponse = rpcNode->forwardNotifyAddFriend(notifyRequest);

    if (notifyResponse.status() == "success") {
      rsp["error"] = ErrorCodes::Success;
    } else {
      rsp["message"] = "RPC转发异常";
      rsp["error"] = ErrorCodes::RPCFailed;
    }
    return rsp;
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
}

Json::Value ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request) {
  Json::Value rsp;

  long from = request["from"].asInt64();
  long to = request["to"].asInt64();
  bool accept = request["accept"].asBool();
  std::string replyMessage = request["replyMessage"].asString();
  bool skipStorage = request.get("__skipStorage", false).asBool();

  rsp["uid"] = Json::Value::Int64(to);

  long sessionKey = skipStorage ? request.get("sessionKey", 0).asInt64()
                                : StoreageReplyAddFriend(request);
  if (sessionKey == -1) {
    LOG_ERROR(businessLogger,
              "StoreageReplyAddFriend is failed, from {}, to {}", from, to);
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库修改发送异常";
    return rsp;
  }

  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  if (isLocalMachineOnline) {
    Json::Value senderRsp{};
    senderRsp["from"] = Json::Value::Int64(from);
    senderRsp["to"] = Json::Value::Int64(to);
    senderRsp["uid"] = Json::Value::Int64(to);
    senderRsp["sessionKey"] = Json::Value::Int64(sessionKey);
    senderRsp["accept"] = accept;
    senderRsp["replyMessage"] = replyMessage;

    auto toSession = OnlineUser::GetInstance()->GetUserSession(to);
    toSession->Send(senderRsp.toStyledString(), ID_REPLY_ADD_FRIEND_REQ);

    rsp["error"] = ErrorCodes::Success;
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
    auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
    if (rpcNode == nullptr) {
      rsp["error"] = ErrorCodes::RPCFailed;
      rsp["message"] = "RPC目标机器不存在";
      return rsp;
    }
    replyResponse = rpcNode->forwardReplyAddFriend(replyRequest);
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
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
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
    sessionId = db::RedisDao::GetInstance()->generateSessionId();
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
};  // namespace wim
