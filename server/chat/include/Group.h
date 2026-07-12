#pragma once

#include "ChatSession.h"
#include "TcpMessageCodec.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wim {

TcpPacket GroupCreate(ChatSession::Ptr session, unsigned int msgID,
                      TcpPacket &request);
TcpPacket GroupNotifyJoin(ChatSession::Ptr session, unsigned int msgID,
                          TcpPacket &request);

TcpPacket GroupPullNotify(ChatSession::Ptr session, unsigned int msgID,
                          TcpPacket &request);

TcpPacket GroupReplyJoin(ChatSession::Ptr session, unsigned int msgID,
                         TcpPacket &request);
TcpPacket GroupQuit(ChatSession::Ptr session, unsigned int msgID,
                    TcpPacket &request);
TcpPacket GroupTextSend(ChatSession::Ptr session, unsigned int msgID,
                        TcpPacket &request);
int NotifyMemberJoin(int64_t uid, int64_t gid,
                     const std::string &requestMessage);
int NotifyMemberReply(int64_t gid, int64_t managerUid, int64_t requestorUid,
                      bool accept);
};  // namespace wim
