#include "DeliveryService.h"

#include "Const.h"
#include "GatewayStreamService.h"
#include "Redis.h"

#include <utility>

namespace wimi {

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

void DeliveryService::Acknowledge(int64_t uid, int64_t seq) const {
  (void)uid;
  (void)seq;
}

}  // namespace wimi
