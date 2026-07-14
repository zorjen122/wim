#pragma once

#include "ChatSession.h"
#include "TcpMessageCodec.h"

namespace wim {

class FileService {
 public:
  TcpPacket Upload(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request);
};

}  // namespace wim
