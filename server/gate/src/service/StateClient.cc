#include "StateClient.h"
#include "Configer.h"
#include "state.grpc.pb.h"
#include <grpcpp/client_context.h>
StateClient::StateClient() {

  auto conf = Configer::getConfig("server");

  if (conf.IsNull())
    spdlog::error("Configer::getConfig(\"Server\") failed");

  auto host = conf["stateRPC"]["host"].as<std::string>();
  auto port = conf["stateRPC"]["port"].as<std::string>();
  auto rpcCount = conf["stateRPC"]["rpcCount"].as<int>();
  pool.reset(new RpcPool<StateService>(rpcCount, host, port));
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
