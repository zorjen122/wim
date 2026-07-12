#pragma once
#include "ChatSession.h"
#include "TcpMessageCodec.h"

namespace wim {

TcpPacket NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                          TcpPacket &request);
int StoreageNotifyAddFriend(TcpPacket &request);

TcpPacket ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                         TcpPacket &request);
int StoreageReplyAddFriend(TcpPacket &request);

void RemoveFriend(ChatSession::Ptr session, unsigned int msgID,
                  TcpPacket &request);
};  // namespace wim
