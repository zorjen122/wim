#pragma once

#include "TcpMessageCodec.h"

namespace wim {

class FileService {
 public:
  TcpPacket Upload(uint32_t msgID, TcpPacket &request);
};

}  // namespace wim
