#include "RpcService.h"
#include "Configer.h"
#include "ImActiver.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <jsoncpp/json/value.h>

#include "Friend.h"

namespace wim::rpc {
// ImRpcService.cpp
grpc::Status ImRpcService::ActiveService(ServerContext *context,
                                         const ActiveRequest *request,
                                         ActiveResponse *response) {
  // 废弃
  return grpc::Status::OK;
}

grpc::Status
ImRpcService::NotifyAddFriend(ServerContext *context,
                              const NotifyAddFriendRequest *request,
                              NotifyAddFriendResponse *response) {
  long fromUid = request->fromuid();
  long toUid = request->touid();
  std::string message = request->requestmessage();

  LOG_DEBUG(wim::netLogger, "NotifyAddFriend, from:{}, to:{}", fromUid, toUid);
  bool isOnlineUser = OnlineUser::GetInstance()->isOnline(toUid);
  if (!isOnlineUser) {
    LOG_INFO(wim::netLogger, "ID为{}的用户已下线", fromUid);
    db::FriendApply::Ptr applyData(new db::FriendApply(fromUid, toUid));
    db::MysqlDao::GetInstance()->insertFriendApply(applyData);
    return grpc::Status::OK;
  }

  Json::Value requestData;
  requestData["from"] = Json::Value::Int64(fromUid);
  requestData["to"] = Json::Value::Int64(toUid);
  auto toSession = OnlineUser::GetInstance()->GetUserSession(toUid);
  int status = wim::OnlineNotifyAddFriend(toSession, requestData);
  if (status == false) {
    LOG_DEBUG(wim::businessLogger, "在线推送通知添加好友异常, from:{}, to:{}",
              fromUid, toUid);
    return grpc::Status::CANCELLED;
  }

  // 存储，保险方案
  db::FriendApply::Ptr applyData(new db::FriendApply(fromUid, toUid));
  status = db::MysqlDao::GetInstance()->insertFriendApply(applyData);

  LOG_DEBUG(wim::businessLogger, "在线推送通知添加好友成功, from:{}, to:{}",
            fromUid, toUid);
  return grpc::Status::OK;
}
grpc::Status ImRpcService::ReplyAddFriend(ServerContext *context,
                                          const ReplyAddFriendRequest *request,
                                          ReplyAddFriendResponse *response) {
  return grpc::Status::OK;
}

grpc::Status
ImRpcService::TextSendMessage(ServerContext *context,
                              const TextSendMessageRequest *request,
                              TextSendMessageResponse *response) {
  return grpc::Status::OK;
}
}; // namespace wim::rpc