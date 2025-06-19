#include "StateRpc.h"
#include "Configer.h"
#include "Const.h"
#include "Logger.h"
#include "spdlog/logger.h"
#include "state.grpc.pb.h"
#include <grpcpp/client_context.h>

namespace wim::rpc {

StateRpc::StateRpc() {

  auto conf = Configer::getNode("server");
  auto host = conf["state"]["host"].as<std::string>();
  auto port = conf["state"]["port"].as<std::string>();
  auto rpcCount = conf["state"]["rpcCount"].as<int>();
  rpcPool.reset(new RpcPool<state::StateService>(rpcCount, host, port));

  if (rpcPool->empty()) {
    netLogger->warn("状态RPC服务启动失败, 地址：{}，端口：{}，rpc连接数：{}",
                    host, port, rpcCount);
    return;
  }

  netLogger->info("状态RPC服务启动成功, 地址：{}，端口：{}，rpc连接数：{}",
                  host, port, rpcCount);
}

EndPoint StateRpc::GetImNode(int uid) {
  UserId req;
  EndPoint rsp;
  req.set_uid(uid);
  auto caller = rpcPool->getConnection();

  grpc::ClientContext context;
  auto status = caller->GetImNode(&context, req, &rsp);
  if (!status.ok()) {
    LOG_WARN(netLogger, "请求IM节点失败, uid: {}", uid);
    return EndPoint();
  }
  return rsp;
}

EndPointList StateRpc::PullImNodeList() {
  Empty req;
  EndPointList rsp;
  auto caller = rpcPool->getConnection();

  grpc::ClientContext context;
  auto status = caller->PullImNodeList(&context, req, &rsp);
  if (!status.ok()) {
    LOG_WARN(netLogger, "拉取IM节点列表失败");
    return EndPointList();
  }
  return rsp;
}

std::string StateRpc::TestNetworkPing() {
  TestNetwork req, rsp;
  req.set_msg("Ping");
  auto caller = rpcPool->getConnection();
  Defer defer(
      [&caller, this]() { rpcPool->returnConnection(std::move(caller)); });
  grpc::ClientContext context;
  LOG_DEBUG(netLogger, "TestNetworkPing: {}", req.msg());
  auto status = caller->TestNetworkPing(&context, req, &rsp);
  LOG_DEBUG(netLogger, "TestNetworkPing rsp: {}", rsp.msg());
  if (status.ok()) {
    return rsp.msg();
  } else {
    return "";
  }
}
}; // namespace wim::rpc