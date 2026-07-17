#include "DeliveryService.h"

#include "Const.h"
#include "GatewayStreamService.h"
#include "ImRpc.h"
#include "OnlineUser.h"
#include "Redis.h"
#include "TcpMessageCodec.h"

#include <jsoncpp/json/value.h>
#include <utility>

namespace wim {

void DeliveryService::SetGatewayStreamService(
    rpc::GatewayStreamService *service) {
  gatewayStreamService.store(service, std::memory_order_release);
}

bool DeliveryService::SendGateway(int64_t uid, const std::string &packet,
                                  uint32_t protocolId, int64_t deliveryId,
                                  int64_t messageId, int64_t conversationId,
                                  int64_t conversationSeq) const {
  auto *service = gatewayStreamService.load(std::memory_order_acquire);
  if (!service)
    return false;
  if (deliveryId <= 0)
    deliveryId = db::RedisDao::GetInstance()->generateMsgId();
  gateway::DeliveryEnvelope delivery;
  delivery.set_delivery_id(std::to_string(protocolId) + ":" +
                           std::to_string(deliveryId) + ":" +
                           std::to_string(uid));
  delivery.set_recipient_uid(uid);
  delivery.set_protocol_id(protocolId);
  delivery.set_message_id(messageId);
  delivery.set_conversation_id(conversationId);
  delivery.set_conversation_seq(conversationSeq);
  delivery.set_packet(packet);
  return service->DeliverToUser(uid, std::move(delivery));
}

DeliveryService::Target DeliveryService::Locate(int64_t uid) const {
  if (OnlineUser::GetInstance()->isOnline(uid)) {
    return {Location::Local, {}};
  }

  Json::Value userInfo =
      db::RedisDao::GetInstance()->getOnlineUserInfoObject(uid);
  if (!userInfo.empty()) {
    return {Location::Remote, userInfo["machineId"].asString()};
  }

  return {Location::Offline, {}};
}

bool DeliveryService::SendLocal(int64_t uid, const std::string &packet,
                                uint32_t protocolId) const {
  auto session = OnlineUser::GetInstance()->GetUserSession(uid);
  if (!session) {
    return false;
  }
  session->Send(packet, protocolId);
  return true;
}

bool DeliveryService::SendLocalReliable(int64_t uid, int64_t seq,
                                        const std::string &packet,
                                        uint32_t protocolId) const {
  if (!OnlineUser::GetInstance()->isOnline(uid)) {
    return false;
  }
  OnlineUser::GetInstance()->onReWrite(OnlineUser::ReWriteType::Message, seq,
                                       uid, packet, protocolId);
  return true;
}

bool DeliveryService::DeliverFriendApplyLocal(
    int64_t from, int64_t to, const std::string &message) const {
  TcpPacket packet;
  packet.set_uid(from);
  packet.set_from(from);
  packet.set_to(to);
  packet.set_request_message(message);
  return SendLocal(to, SerializeTcpPacket(packet), ID_NOTIFY_ADD_FRIEND_REQ);
}

bool DeliveryService::DeliverFriendReplyLocal(
    int64_t from, int64_t to, bool accept, const std::string &message) const {
  TcpPacket packet;
  packet.set_from(from);
  packet.set_to(to);
  packet.set_uid(to);
  packet.set_session_key(0);
  packet.set_accept(accept);
  packet.set_reply_message(message);
  return SendLocal(to, SerializeTcpPacket(packet), ID_REPLY_ADD_FRIEND_REQ);
}

void DeliveryService::Acknowledge(int64_t uid, int64_t seq) const {
  OnlineUser::GetInstance()->cancelAckTimer(seq, uid);
}

DeliveryService::RemoteResult DeliveryService::ForwardFriendApply(
    const std::string &machineId, int64_t from, int64_t to,
    const std::string &message) const {
  auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
  if (!rpcNode) {
    return {};
  }

  rpc::NotifyAddFriendRequest request;
  request.set_from(from);
  request.set_to(to);
  request.set_requestmessage(message);
  auto response = rpcNode->forwardNotifyAddFriend(request);
  return {true, response.status() == "success", ErrorCodes::RPCFailed, 0,
          response.status()};
}

DeliveryService::RemoteResult DeliveryService::ForwardFriendReply(
    const std::string &machineId, int64_t from, int64_t to, bool accept,
    const std::string &message) const {
  auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
  if (!rpcNode) {
    return {};
  }

  rpc::ReplyAddFriendRequest request;
  request.set_from(from);
  request.set_to(to);
  request.set_accept(accept);
  request.set_replymessage(message);
  auto response = rpcNode->forwardReplyAddFriend(request);
  return {true, response.status() == "success", ErrorCodes::RPCFailed, 0,
          response.status()};
}

DeliveryService::RemoteResult DeliveryService::ForwardText(
    const std::string &machineId, int64_t from, int64_t to,
    const std::string &text, int64_t seq, int64_t sessionKey,
    const std::string &requestId) const {
  auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
  if (!rpcNode) {
    return {};
  }

  rpc::TextSendMessageRequest request;
  request.set_from(from);
  request.set_to(to);
  request.set_text(text);
  request.set_seq(seq);
  request.set_session_key(sessionKey);
  request.set_request_id(requestId);
  auto response = rpcNode->forwardTextSendMessage(request);
  int error = response.error() == ErrorCodes::Success ? ErrorCodes::RPCFailed
                                                      : response.error();
  return {true, response.status() == "success", error, response.message_id(),
          response.status()};
}

}  // namespace wim
