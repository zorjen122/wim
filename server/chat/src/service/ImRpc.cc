#include "ImRpc.h"
#include "Configer.h"
#include "Const.h"
#include "Logger.h"
#include "RpcPool.h"
#include "im.pb.h"
#include <grpcpp/support/status.h>
#include <string>

namespace wim::rpc {

ImRpcNode::ImRpcNode(const std::string &ip, unsigned short port,
                     size_t poolSize) {
  pool.reset(new RpcPool<ImService>(poolSize, ip, std::to_string(port)));
  if (pool->empty()) {
    LOG_WARN(netLogger, "Failed to create ImRpcNode");
  }
  LOG_DEBUG(netLogger, "Created ImRpcNode with ip: {}, port: {}, poolSize: {}",
            ip, port, poolSize);
}
NotifyAddFriendResponse
ImRpcNode::forwardNotifyAddFriend(const NotifyAddFriendRequest &request) {

  auto rpc = pool->getConnection();
  if (!rpc)
    return NotifyAddFriendResponse();

  Defer defer([this, &rpc] { pool->returnConnection(std::move(rpc)); });

  grpc::ClientContext context;
  NotifyAddFriendResponse response;

  grpc::Status status = rpc->NotifyAddFriend(&context, request, &response);
  if (status.ok()) {
    response.set_status("success");
  } else {
    response.set_status("failed");
    LOG_DEBUG(netLogger, "Failed to forward NotifyAddFriendRequest: {}",
              status.error_message());
  }

  return response;
}

ReplyAddFriendResponse
ImRpcNode::forwardReplyAddFriend(const ReplyAddFriendRequest &) {}
TextSendMessageResponse
ImRpcNode::forwardTextSendMessage(const TextSendMessageRequest &) {}

ImRpc::ImRpc() {
  auto conf = Configer::getNode("server");

  auto imTotal = conf["im"]["im-total"].as<int>();
  for (int i = 1; i <= imTotal; i++) {
    std::string index = "s" + std::to_string(i);
    auto im = conf["im"][index];
    Machinekey name = static_cast<Machinekey>(im["name"].as<std::string>());
    auto host = im["host"].as<std::string>();
    auto rpcPort = im["rpcPort"].as<int>();
    auto rpcCount = im["rpcCount"].as<int>();
    rpcGroup[name] = ImRpcNode::Ptr(new ImRpcNode(host, rpcPort, rpcCount));

    spdlog::info("ImRpcNode[{}] {}:{} {}", index, host, rpcPort, name);
  }
}
ImRpcNode::Ptr ImRpc::getRpc(const Machinekey &machine) {
  auto it = rpcGroup.find(machine);
  if (it == rpcGroup.end()) {
    return nullptr;
  }
  return it->second;
}

ImRpc::Machinekey ImRpc::getMachineKey(long hashValue) {
  hashValue = hashValue % rpcGroup.size();
  auto it = rpcGroup.begin();
  std::advance(it, hashValue);

  Machinekey key = it->first;
  LOG_DEBUG(netLogger, "getMachineKey: hash {} -> {}", hashValue, key);
  return key;
}

ImRpcNode::Ptr ImRpc::getRpc(long hashValue) {
  Machinekey key = getMachineKey(hashValue);

  return getRpc(key);
}

}; // namespace wim::rpc