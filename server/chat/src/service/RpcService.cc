#include "RpcService.h"
#include "Configer.h"
#include "ImActiver.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "json/value.h"
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include "Friend.h"

namespace wim::rpc {
// ImRpcService.cpp
grpc::Status ImRpcService::ActiveService(ServerContext *context,
                                         const ActiveRequest *request,
                                         ActiveResponse *response) {
  bool success =
      ImServiceRunner::GetInstance()->Activate(ImServiceRunner::BACKUP_ACTIVE);
  if (!success) {
    response->set_error("failed");
    return grpc::Status::CANCELLED;
  }

  response->set_error("success");
  return grpc::Status::OK;
}

grpc::Status
ImRpcService::NotifyAddFriend(ServerContext *context,
                              const NotifyAddFriendRequest *request,
                              NotifyAddFriendResponse *response) {
  long fromUid = request->fromuid();
  long toUid = request->touid();
  std::string message = request->requestmessage();

  bool isOnlineUser = OnlineUser::GetInstance()->isOnline(toUid);
  if (!isOnlineUser) {
    // 转存储
    db::FriendApply::Ptr applyData(new db::FriendApply(fromUid, toUid));
    db::MysqlDao::GetInstance()->insertFriendApply(applyData);

    return grpc::Status::OK;
  }

  auto user = OnlineUser::GetInstance()->GetUser(toUid);
  Json::Value requestData;
  requestData["from"] = Json::Value::Int64(fromUid);
  requestData["to"] = Json::Value::Int64(toUid);
  int status = wim::OnlineNotifyAddFriend(user, requestData);
  if (status != 0) {
    LOG_DEBUG(wim::businessLogger,
              "forward NotifyAddFriend failed, from:{}, to:{}", fromUid, toUid);
    return grpc::Status::CANCELLED;
  }
  // 存储，保险方案
  db::FriendApply::Ptr applyData(new db::FriendApply(fromUid, toUid));
  db::MysqlDao::GetInstance()->insertFriendApply(applyData);

  LOG_DEBUG(wim::businessLogger,
            "forward NotifyAddFriend success, from:{}, to:{}", fromUid, toUid);
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