#include "RpcService.h"
#include "Configer.h"
#include "Const.h"
#include "ImActiver.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "RequestContext.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <cstddef>
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include "Friend.h"
#include "Service.h"

namespace wim::rpc {
namespace {

RequestContext MakeRpcContext(ServerContext *serverContext,
                              std::string requestId,
                              std::string operation, int64_t actor) {
  auto deadline = RequestContext::Deadline::max();
  auto grpcDeadline = serverContext->deadline();
  if (grpcDeadline != std::chrono::system_clock::time_point::max()) {
    auto remaining = grpcDeadline - std::chrono::system_clock::now();
    deadline = RequestContext::Clock::now() +
               std::max(remaining, std::chrono::system_clock::duration::zero());
  }
  if (requestId.empty()) {
    requestId = RequestContext::NextRequestId();
  }
  return RequestContext(std::move(requestId), std::move(operation),
                        RequestSource::Rpc, actor, deadline);
}

}  // namespace
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
  auto requestContext = MakeRpcContext(
      context, {}, "NotifyAddFriend", static_cast<int64_t>(from));
  RequestContextScope contextScope(requestContext);
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
  auto requestContext = MakeRpcContext(
      context, {}, "ReplyAddFriend", static_cast<int64_t>(from));
  RequestContextScope contextScope(requestContext);
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
  auto requestContext = MakeRpcContext(context, request->request_id(),
                                       "TextSendMessage", from);
  RequestContextScope contextScope(requestContext);
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

  // 仅供故障测试：模拟目标节点已经接收消息，但 RPC 响应超过调用方 deadline。
  // 后续重试必须由持久幂等返回同一个 message_id。
  if (const char *delayPayload =
          std::getenv("WIM_CHAT_TEST_RPC_DELAY_AFTER_ACCEPT_PAYLOAD");
      delayPayload != nullptr && text == delayPayload) {
    int delayMs = 0;
    if (const char *configured =
            std::getenv("WIM_CHAT_TEST_RPC_DELAY_AFTER_ACCEPT_MS")) {
      delayMs = std::max(0, std::atoi(configured));
    }
    LOG_WARN(businessLogger,
             "触发测试RPC延迟：消息已接收, requestId: {}, delayMs: {}",
             requestContext.requestId, delayMs);
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
  }

  int ec = TcpPacketError(localServiceResponse);
  response->set_error(ec);
  if (localServiceResponse.has_message_id())
    response->set_message_id(localServiceResponse.message_id());
  if (ec != 0) {
    LOG_INFO(businessLogger, "error: {}", ec);
    response->set_status("failed");
    return grpc::Status::OK;
  }

  response->set_status("success");
  return grpc::Status::OK;
}
};  // namespace wim::rpc
