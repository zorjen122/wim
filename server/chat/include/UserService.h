#pragma once

#include "ChatSession.h"
#include "TcpMessageCodec.h"

namespace wim {

class UserService {
 public:
  TcpPacket Login(ChatSession::Ptr session, uint32_t msgID, TcpPacket &request);
  TcpPacket Quit(ChatSession::Ptr session, uint32_t msgID, TcpPacket &request);
  TcpPacket Search(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request);

 private:
  TcpPacket ReLogin(int64_t uid, ChatSession::Ptr oldSession,
                    ChatSession::Ptr newSession);
};

}  // namespace wim
