#include "StateClient.h"
#include "Configer.h"
#include "Const.h"
#include "Logger.h"
#include "spdlog/logger.h"
#include "state.grpc.pb.h"
#include <grpcpp/client_context.h>

namespace wim::rpc {

StateClient::StateClient() {

  auto conf = Configer::getConfig("server");
  auto host = conf["stateRPC"]["host"].as<std::string>();
  auto port = conf["stateRPC"]["port"].as<std::string>();
  auto rpcCount = conf["stateRPC"]["rpcCount"].as<int>();
  pool.reset(new RpcPool<StateService>(rpcCount, host, port));

  if (pool->empty()) {
    netLogger->warn(
        "StateClient init failed, host: {}, port: {}, rpcCount: {}");
    return;
  }
  netLogger->info("StateClient init success, host: {}, port: {}, rpcCount: {}",
                  host, port, pool->getPoolSize());
}

ServerNode StateClient::GetImServer(int uid) {
  state::ConnectUser req;
  state::ConnectUserRsp rsp;
  req.set_id(uid);
  auto caller = pool->getConnection();

  grpc::ClientContext context;
  auto status = caller->GetImServer(&context, req, &rsp);
  if (status.ok()) {
    ServerNode node(rsp.ip(), rsp.port());
    return node;
  } else {
    return ServerNode();
  }
}

ServerNode StateClient::ActiveImBackupServer(int uid) {
  state::ConnectUser req;
  state::ConnectUserRsp rsp;
  req.set_id(uid);
  auto caller = pool->getConnection();

  grpc::ClientContext context;
  auto status = caller->ActiveImBackupServer(&context, req, &rsp);
  if (status.ok()) {
    ServerNode node(rsp.ip(), rsp.port());
    return node;
  } else {
    return ServerNode();
  }
}

std::string StateClient::TestNetworkPing() {
  state::TestNetwork req, rsp;
  req.set_msg("Ping");
  auto caller = pool->getConnection();
  Defer defer([&caller, this]() { pool->returnConnection(std::move(caller)); });
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