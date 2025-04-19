#pragma once
#include "global.h"
#include "imRpc.h"
#include "state.grpc.pb.h"
#include "state.pb.h"

namespace wim::rpc {
using state::ConnectUser;
using state::ConnectUserRsp;
using state::StateService;
using state::TestNetwork;

class StateServiceImpl final : public StateService::Service {
public:
  StateServiceImpl();
  grpc::Status GetImServer(grpc::ServerContext *context,
                           const ConnectUser *request,
                           ConnectUserRsp *response) override;

  grpc::Status ActiveImBackupServer(grpc::ServerContext *context,
                                    const ConnectUser *request,
                                    ConnectUserRsp *response) override;

  grpc::Status TestNetworkPing(grpc::ServerContext *context,
                               const TestNetwork *request,
                               TestNetwork *response) override;

  std::unordered_map<std::string, ImNode::ptr> imNodeMap;
  std::unordered_map<std::string, std::unique_ptr<ImRpc>> imRpcMap;
  std::vector<std::string> imNodeName;
};
}; // namespace wim::rpc