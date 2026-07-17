#pragma once

#include "TcpMessageCodec.h"

#include <vector>

namespace wim {

class DeliveryService;

class MessageService {
 public:
  struct AcceptedText {
    TcpPacket response;
    TcpPacket delivery;
    int64_t recipientUid{0};
    bool shouldDeliver{false};
  };

  struct AcceptedGroupText {
    TcpPacket response;
    TcpPacket delivery;
    std::vector<int64_t> recipientUids;
    bool shouldDeliver{false};
  };

  explicit MessageService(DeliveryService &deliveryService);

  AcceptedText AcceptText(TcpPacket request);
  AcceptedGroupText AcceptGroupText(TcpPacket request);

  TcpPacket Ack(uint32_t msgID, TcpPacket &request);
  TcpPacket SendText(uint32_t msgID, TcpPacket &request);
  TcpPacket SendFile(uint32_t msgID, TcpPacket &request);
  TcpPacket SendGroupText(uint32_t msgID, TcpPacket &request);
  TcpPacket PullSessionMessages(uint32_t msgID, TcpPacket &request);
  TcpPacket PullMessages(uint32_t msgID, TcpPacket &request);

 private:
  DeliveryService &deliveryService;
};

}  // namespace wim
