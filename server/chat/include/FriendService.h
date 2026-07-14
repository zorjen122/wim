#pragma once

#include "ChatSession.h"
#include "TcpMessageCodec.h"

namespace wim {

class DeliveryService;

class FriendService {
 public:
  explicit FriendService(DeliveryService &deliveryService);

  TcpPacket NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                            TcpPacket &request);
  TcpPacket ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                           TcpPacket &request);
  TcpPacket PullFriendApplyList(ChatSession::Ptr session, uint32_t msgID,
                                TcpPacket &request);
  TcpPacket PullFriendList(ChatSession::Ptr session, uint32_t msgID,
                           TcpPacket &request);
  void RemoveFriend(ChatSession::Ptr session, unsigned int msgID,
                    TcpPacket &request);

 private:
  int StoreNotifyAddFriend(TcpPacket &request);
  int StoreReplyAddFriend(TcpPacket &request);

  DeliveryService &deliveryService;
};

}  // namespace wim
