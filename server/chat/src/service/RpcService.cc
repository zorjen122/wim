#include "RpcService.h"
#include "Configer.h"
#include "Const.h"
#include "ImActiver.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include <cstddef>
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <jsoncpp/json/value.h>

#include "Friend.h"
#include "Service.h"

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

  Json::Value requestJsonData;
  requestJsonData["fromUid"] = Json::Value::Int64(fromUid);
  requestJsonData["toUid"] = Json::Value::Int64(toUid);
  requestJsonData["requestMessage"] = Json::Value(message);

  auto localServiceResponse =
      wim::NotifyAddFriend(NULL, ID_NOTIFY_ADD_FRIEND_REQ, requestJsonData);

  int ec = localServiceResponse["error"].asInt();
  if (ec < 0) {
    LOG_INFO(businessLogger, "error: {}", ec);
    return grpc::Status::CANCELLED;
  }
  return grpc::Status::OK;
}

grpc::Status ImRpcService::ReplyAddFriend(ServerContext *context,
                                          const ReplyAddFriendRequest *request,
                                          ReplyAddFriendResponse *response) {
  long fromUid = request->fromuid();
  long toUid = request->touid();
  bool accept = request->accept();
  std::string message = request->replymessage();

  Json::Value requestJsonData;
  requestJsonData["fromUid"] = Json::Value::Int64(fromUid);
  requestJsonData["toUid"] = Json::Value::Int64(toUid);
  requestJsonData["accept"] = Json::Value(accept);
  requestJsonData["replyMessage"] = Json::Value(message);

  auto localServiceResponse =
      wim::ReplyAddFriend(nullptr, ID_REPLY_ADD_FRIEND_REQ, requestJsonData);

  int ec = localServiceResponse["error"].asInt();
  if (ec < 0) {
    LOG_INFO(businessLogger, "error: ", ec);
    return grpc::Status::CANCELLED;
  }
  return grpc::Status::OK;
}

grpc::Status
ImRpcService::TextSendMessage(ServerContext *context,
                              const TextSendMessageRequest *request,
                              TextSendMessageResponse *response) {

  long fromUid = request->fromuid();
  long toUid = request->touid();
  std::string text = request->text();
  Json::Value requestJsonData;
  requestJsonData["fromUid"] = Json::Value::Int64(fromUid);
  requestJsonData["toUid"] = Json::Value::Int64(toUid);
  requestJsonData["text"] = Json::Value(text);
  requestJsonData["sessionKey"] = Json::Value::Int64(0);

  auto localServiceResponse =
      wim::TextSend(nullptr, ID_TEXT_SEND_REQ, requestJsonData);

  int ec = localServiceResponse["error"].asInt();
  if (ec != 0) {
    LOG_INFO(businessLogger, "error: {}", ec);
    return grpc::Status::CANCELLED;
  }

  return grpc::Status::OK;
}
}; // namespace wim::rpc