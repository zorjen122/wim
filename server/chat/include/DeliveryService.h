#pragma once

#include <cstdint>
#include <atomic>
#include <string>

namespace wimi {

namespace rpc {
class GatewayStreamService;
}

class DeliveryService {
 public:
  void SetGatewayStreamService(rpc::GatewayStreamService *service);
  bool SendGateway(int64_t uid, const std::string &packet, uint32_t protocolId,
                   int64_t deliveryId = 0, int64_t messageId = 0,
                   int64_t conversationId = 0,
                   int64_t conversationSeq = 0) const;
  void Acknowledge(int64_t uid, int64_t seq) const;

 private:
  std::atomic<rpc::GatewayStreamService *> gatewayStreamService{nullptr};
};

}  // namespace wimi
