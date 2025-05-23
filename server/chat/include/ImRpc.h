#pragma once
#include "Const.h"
#include "RpcPool.h"
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpc/grpc.h>
#include <string>
#include <unordered_map>

namespace wim::rpc {
using im::NotifyAddFriendRequest;
using im::NotifyAddFriendResponse;

using im::ReplyAddFriendRequest;
using im::ReplyAddFriendResponse;

using im::TextSendMessageRequest;
using im::TextSendMessageResponse;

using im::ImService;

class ImRpcNode {
public:
  using Ptr = std::shared_ptr<ImRpcNode>;
  ImRpcNode(const std::string &ip, unsigned short port, size_t poolSize);
  NotifyAddFriendResponse
  forwardNotifyAddFriend(const NotifyAddFriendRequest &);
  ReplyAddFriendResponse forwardReplyAddFriend(const ReplyAddFriendRequest &);
  TextSendMessageResponse
  forwardTextSendMessage(const TextSendMessageRequest &);

private:
  std::unique_ptr<RpcPool<ImService>> pool = nullptr;
};

class ImRpc : public Singleton<ImRpc> {
public:
  using Machinekey = std::string;
  ImRpc();
  ImRpcNode::Ptr getRpc(const Machinekey &machine);
  ImRpcNode::Ptr getRpc(long hashValue);

private:
  Machinekey getMachineKey(long hashValue);

private:
  std::unordered_map<Machinekey, ImRpcNode::Ptr> rpcGroup;
};

}; // namespace wim::rpc