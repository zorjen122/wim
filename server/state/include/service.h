#pragma once
#include "global.h"
#include "imRpc.h"
#include "state.grpc.pb.h"
#include "state.pb.h"

#include <atomic>

namespace wimi::rpc {
using state::ConnectUser;
using state::ConnectUserRsp;
using state::MessageTopology;
using state::StateService;
using state::TestNetwork;
using state::TopologyRequest;

class StateServiceImpl final : public StateService::Service {
 public:
  StateServiceImpl();
  grpc::Status PickConnectionGateway(grpc::ServerContext *context,
                                     const ConnectUser *request,
                                     ConnectUserRsp *response) override;

  grpc::Status ListMessageNodes(grpc::ServerContext *context,
                                const TopologyRequest *request,
                                MessageTopology *response) override;

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
  std::vector<ServiceNodeInfo> gatewayNodes;
  std::vector<ServiceNodeInfo> messageNodes;
  std::atomic<std::size_t> gatewayRouteCount{0};
  std::uint64_t topologyVersion{1};
};
};  // namespace wimi::rpc
