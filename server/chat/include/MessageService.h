#pragma once

#include "ChatSession.h"
#include "TcpMessageCodec.h"

namespace wim {

class DeliveryService;

class MessageService {
 public:
  explicit MessageService(DeliveryService &deliveryService);

  TcpPacket Ack(ChatSession::Ptr session, uint32_t msgID, TcpPacket &request);
  TcpPacket SendText(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request);
  TcpPacket SendFile(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request);
  TcpPacket SendGroupText(ChatSession::Ptr session, uint32_t msgID,
                          TcpPacket &request);
  TcpPacket PullSessionMessages(ChatSession::Ptr session, uint32_t msgID,
                                TcpPacket &request);
  TcpPacket PullMessages(ChatSession::Ptr session, uint32_t msgID,
                         TcpPacket &request);

 private:
  DeliveryService &deliveryService;
};

}  // namespace wim
