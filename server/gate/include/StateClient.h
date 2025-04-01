#pragma once
#include <grpcpp/grpcpp.h>

#include "Const.h"
#include "RpcPool.h"
#include "state.grpc.pb.h"
#include "state.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using state::ConnectUser;
using state::ConnectUserRsp;
using state::StateService;

class ServerNode {
public:
  ServerNode() = default;
  ServerNode(std::string ip, int port) : ip(ip), port(port) {}
  std::string ip;
  unsigned short port;
};

class StateClient : public Singleton<StateClient> {
public:
  ServerNode GetImServer(int uid);
  StateClient();

private:
  std::unique_ptr<RpcPool<StateService>> pool = nullptr;
};