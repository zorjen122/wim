#pragma once
#include "global.h"
#include "imRpc.h"
#include "state.grpc.pb.h"
#include "state.pb.h"

namespace wim::rpc {
using state::Empty;
using state::EndPoint;
using state::EndPointList;
using state::StateService;
using state::TestNetwork;
using state::UserId;

class StateServiceImpl final : public StateService::Service {
public:
  StateServiceImpl();
  grpc::Status GetImNode(grpc::ServerContext *context, const UserId *request,
                         EndPoint *response) override;
  grpc::Status PullImNodeList(grpc::ServerContext *context,
                              const Empty *request,
                              EndPointList *response) override;

  grpc::Status TestNetworkPing(grpc::ServerContext *context,
                               const TestNetwork *request,
                               TestNetwork *response) override;

  std::unordered_map<std::string, ImNode::ptr> imNodeMap;
  std::unordered_map<std::string, std::unique_ptr<ImRpc>> imRpcMap;
  std::vector<std::string> imNodeName;
};
}; // namespace wim::rpc