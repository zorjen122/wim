#include "Friend.h"
#include "DbGlobal.h"
#include "ImRpc.h"

#include "Const.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"

#include <jsoncpp/json/value.h>
#include <spdlog/spdlog.h>
#include <string>
namespace wim {

int StoreageNotifyAddFriend(TcpPacket &request) {
  long from = request.uid();
  long to = request.to();
  std::string requestMessage = request.request_message();
  db::FriendApply::Ptr friendApply(new db::FriendApply(
      from, to, db::FriendApply::Status::Wait, requestMessage));

  int ret = db::MysqlDao::GetInstance()->insertFriendApply(friendApply);
  return ret;
}

TcpPacket NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                          TcpPacket &request) {
  TcpPacket rsp;

  long from = request.uid();
  long to = request.to();
  bool skipStorage = request.skip_storage();
  // actor 只取 canonical uid，from 仅作为接收端兼容展示字段。
  request.set_from(from);

  int ret = skipStorage ? 0 : StoreageNotifyAddFriend(request);
  rsp.set_uid(to);
  if (ret == -1) {
    LOG_INFO(businessLogger,
             "StoreageNotifyAddFriend save service failed, from-{}, to-{}",
             from, to);
    rsp.set_error(ErrorCodes::MysqlFailed);
    rsp.set_message("数据库修改异常");
    return rsp;
  }

  auto toSession = OnlineUser::GetInstance()->GetUserSession(to);
  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  if (isLocalMachineOnline) {
    // 本地在线推送
    TcpPacket senderRsp = request;
    senderRsp.clear_skip_storage();
    toSession->Send(SerializeTcpPacket(senderRsp), ID_NOTIFY_ADD_FRIEND_REQ);

    rsp.set_error(ErrorCodes::Success);
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
    notifyRequest.set_requestmessage(request.request_message());

    // 通过MachineID路由到对应的机器，并转发
    LOG_INFO(wim::businessLogger,
             "forwardNotifyAddFriend(from: {}, to: {}) to machine: {}", from,
             to, machineId);
    auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
    if (rpcNode == nullptr) {
      rsp.set_message("RPC目标机器不存在");
      rsp.set_error(ErrorCodes::RPCFailed);
      return rsp;
    }
    notifyResponse = rpcNode->forwardNotifyAddFriend(notifyRequest);

    if (notifyResponse.status() == "success") {
      rsp.set_error(ErrorCodes::Success);
    } else {
      rsp.set_message("RPC转发异常");
      rsp.set_error(ErrorCodes::RPCFailed);
    }
    return rsp;
  }

  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

TcpPacket ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                         TcpPacket &request) {
  TcpPacket rsp;

  long from = request.uid();
  long to = request.to();
  bool accept = request.accept();
  std::string replyMessage = request.reply_message();
  bool skipStorage = request.skip_storage();
  // RPC/TCP 入口都已规范化 uid，业务层不再从 session 推导身份。
  request.set_from(from);

  rsp.set_uid(to);

  long sessionKey =
      skipStorage ? request.session_key() : StoreageReplyAddFriend(request);
  if (sessionKey == -1) {
    LOG_ERROR(businessLogger,
              "StoreageReplyAddFriend is failed, from {}, to {}", from, to);
    rsp.set_error(ErrorCodes::MysqlFailed);
    rsp.set_message("数据库修改发送异常");
    return rsp;
  }

  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  if (isLocalMachineOnline) {
    TcpPacket senderRsp{};
    senderRsp.set_from(from);
    senderRsp.set_to(to);
    senderRsp.set_uid(to);
    senderRsp.set_session_key(sessionKey);
    senderRsp.set_accept(accept);
    senderRsp.set_reply_message(replyMessage);

    auto toSession = OnlineUser::GetInstance()->GetUserSession(to);
    toSession->Send(SerializeTcpPacket(senderRsp), ID_REPLY_ADD_FRIEND_REQ);

    rsp.set_error(ErrorCodes::Success);
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
      rsp.set_error(ErrorCodes::RPCFailed);
      rsp.set_message("RPC目标机器不存在");
      return rsp;
    }
    replyResponse = rpcNode->forwardReplyAddFriend(replyRequest);
    LOG_INFO(
        businessLogger,
        "forwardReplyAddFriend(from: {}, to: {}) to machine: {}, response: {}",
        from, to, machineId, replyResponse.status());

    if (replyResponse.status() == "success") {
      rsp.set_error(ErrorCodes::Success);
    } else {
      rsp.set_error(ErrorCodes::RPCFailed);
      rsp.set_message("RPC转发异常");
    }
    return rsp;
  }

  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

int StoreageReplyAddFriend(TcpPacket &request) {
  long from = request.uid();
  long to = request.to();
  bool accept = request.accept();
  std::string replyMessage = request.reply_message();

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
