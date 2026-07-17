#pragma once

#include "TcpMessageCodec.h"

#include <cstdint>
#include <string>

namespace wim {

class DeliveryService;

class GroupService {
 public:
  explicit GroupService(DeliveryService &deliveryService);

  TcpPacket Create(unsigned int msgID, TcpPacket &request);
  TcpPacket NotifyJoin(unsigned int msgID, TcpPacket &request);
  TcpPacket PullNotify(unsigned int msgID, TcpPacket &request);
  TcpPacket ReplyJoin(unsigned int msgID, TcpPacket &request);
  TcpPacket Quit(unsigned int msgID, TcpPacket &request);

 private:
  int NotifyMemberJoin(int64_t uid, int64_t gid,
                       const std::string &requestMessage);
  int NotifyMemberReply(int64_t gid, int64_t managerUid, int64_t requestorUid,
                        bool accept);

  DeliveryService &deliveryService;
};

}  // namespace wim
