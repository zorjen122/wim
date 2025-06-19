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

using ::state::Empty;
using ::state::EndPoint;
using ::state::EndPointList;
using ::state::TestNetwork;
using ::state::UserId;

class StateRpc : public Singleton<StateRpc> {
public:
  EndPoint GetImNode(int uid);
  EndPointList PullImNodeList();
  StateRpc();
  std::string TestNetworkPing();

private:
  std::unique_ptr<RpcPool<state::StateService>> rpcPool = nullptr;
};

}; // namespace wim::rpc