#pragma once
#include "state.grpc.pb.h"
#include "state.pb.h"
#include <grpcpp/grpcpp.h>

#include "Const.h"
#include "RpcPool.h"

using ::grpc::Channel;
using ::grpc::ClientContext;
using ::grpc::Status;

namespace wim::rpc {
using ::state::ConnectUser;

using ::state::ConnectUserRsp;
using ::state::StateService;

class ServerNode {
public:
  ServerNode() = default;
  ServerNode(std::string ip, int port) : ip(ip), port(port) {}

  bool empty() { return ip.empty() || port == 0; }
  std::string ip;
  unsigned short port;
};

class StateClient : public Singleton<StateClient> {
public:
  ServerNode GetImServer(int uid);
  ServerNode ActiveImBackupServer(int uid);
  StateClient();
  std::string TestNetworkPing();

private:
  std::unique_ptr<RpcPool<StateService>> pool = nullptr;
};

}; // namespace wim::rpc