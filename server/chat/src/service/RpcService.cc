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

#include "Friend.h"
#include "Service.h"

namespace wim::rpc {
// ImRpcService.cpp

// RPC 适配器将来源用户规范化为业务 uid；部署到非可信网络前必须由
// mTLS 节点身份约束哪些 Chat 节点有权声明该来源。
grpc::Status ImRpcService::ActiveService(ServerContext *context,
                                         const ActiveRequest *request,
                                         ActiveResponse *response) {
  // 废弃
  return grpc::Status::OK;
}

grpc::Status ImRpcService::NotifyAddFriend(
    ServerContext *context, const NotifyAddFriendRequest *request,
    NotifyAddFriendResponse *response) {
  long from = request->from();
  long to = request->to();
  std::string message = request->requestmessage();

  TcpPacket requestData;
  requestData.set_uid(from);
  requestData.set_from(from);
  requestData.set_to(to);
  requestData.set_request_message(message);
  requestData.set_skip_storage(true);

  auto localServiceResponse =
      wim::NotifyAddFriend(NULL, ID_NOTIFY_ADD_FRIEND_REQ, requestData);

  int ec = TcpPacketError(localServiceResponse);
  if (ec < 0) {
    LOG_INFO(businessLogger, "error: {}", ec);
    return grpc::Status::CANCELLED;
  }
  return grpc::Status::OK;
}

grpc::Status ImRpcService::ReplyAddFriend(ServerContext *context,
                                          const ReplyAddFriendRequest *request,
                                          ReplyAddFriendResponse *response) {
  long from = request->from();
  long to = request->to();
  bool accept = request->accept();
  std::string message = request->replymessage();

  TcpPacket requestData;
  requestData.set_uid(from);
  requestData.set_from(from);
  requestData.set_to(to);
  requestData.set_accept(accept);
  requestData.set_reply_message(message);
  requestData.set_skip_storage(true);

  auto localServiceResponse =
      wim::ReplyAddFriend(nullptr, ID_REPLY_ADD_FRIEND_REQ, requestData);

  int ec = TcpPacketError(localServiceResponse);
  if (ec < 0) {
    LOG_INFO(businessLogger, "error: ", ec);
    return grpc::Status::CANCELLED;
  }
  return grpc::Status::OK;
}

grpc::Status ImRpcService::TextSendMessage(
    ServerContext *context, const TextSendMessageRequest *request,
    TextSendMessageResponse *response) {
  long from = request->from();
  long to = request->to();
  std::string text = request->text();
  TcpPacket requestData;
  requestData.set_seq(request->seq());
  requestData.set_uid(from);
  requestData.set_from(from);
  requestData.set_to(to);
  requestData.set_data(text);
  requestData.set_session_key(request->session_key());

  auto localServiceResponse =
      wim::TextSend(nullptr, ID_TEXT_SEND_REQ, requestData);

  int ec = TcpPacketError(localServiceResponse);
  response->set_error(ec);
  if (localServiceResponse.has_message_id())
    response->set_message_id(localServiceResponse.message_id());
  if (ec != 0) {
    LOG_INFO(businessLogger, "error: {}", ec);
    return grpc::Status::CANCELLED;
  }

  response->set_status("success");
  return grpc::Status::OK;
}
};  // namespace wim::rpc
