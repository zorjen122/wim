#include "service.h"
#include "Configer.h"
#include "global.h"
#include "im.pb.h"
#include "imRpc.h"
#include "spdlog/spdlog.h"
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <string>

namespace wim::rpc {

StateServiceImpl::StateServiceImpl() {
  auto conf = Configer::getNode("server");

  auto imTotal = conf["im"]["im-total"].as<int>();
  for (int i = 1; i <= imTotal; i++) {
    std::string index = "s" + std::to_string(i);
    auto im = conf["im"][index];
    auto host = im["host"].as<std::string>();
    auto port = im["port"].as<std::string>();
    auto rpcPort = im["rpcPort"].as<std::string>();
    auto name = im["name"].as<std::string>();
    auto status = im["status"].as<std::string>();
    auto rpcCount = im["rpcCount"].as<int>();
    ImNode::ptr node(new ImNode(host, port, rpcPort, status));
    spdlog::info("ImNode({}) {}:{} {} {}", index, host, rpcPort, name, status);

    if (status == "backup" || status == "active") {
      if (status == "backup")
        imRpcMap[name] = std::make_unique<ImRpc>(node, rpcCount);

      imNodeMap[name] = node;
      imNodeName.push_back(name);
    }
  }
}

grpc::Status StateServiceImpl::PullImNodeList(grpc::ServerContext *context,
                                              const Empty *request,
                                              EndPointList *response) {

  int imTotal = imNodeName.size();
  for (int i = 0; i < imTotal; ++i) {
    std::string nodeIndex = imNodeName[i];
    EndPoint *endpoint = response->add_endpoints();
    endpoint->set_ip(imNodeMap[nodeIndex]->getIp());
    endpoint->set_port(atoi(imNodeMap[nodeIndex]->getPort().c_str()));
    endpoint->set_name(nodeIndex);
  }
  return grpc::Status::OK;
}

grpc::Status StateServiceImpl::GetImNode(grpc::ServerContext *context,
                                         const UserId *request,
                                         EndPoint *response) {
  static int routeCount = 0;

  int uid = request->uid();

  int imTotal = imNodeName.size();
  for (int _ = 0; _ < imTotal; ++_) {
    auto nodeIndex = imNodeName[routeCount];

    auto &node = imNodeMap[nodeIndex];
    routeCount = (routeCount + 1 == imTotal) ? 0 : routeCount + 1;

    if (node->getStatus() == "active") {
      response->set_ip(node->getIp());
      response->set_port(atoi(node->getPort().c_str()));
      node->appendConnection(uid);
      break;
    }
  }

  return grpc::Status::OK;
}

grpc::Status StateServiceImpl::TestNetworkPing(grpc::ServerContext *context,
                                               const TestNetwork *request,
                                               TestNetwork *response) {

  spdlog::info("imBackupRpc-TestNetworkPing, req {}", request->msg());

  auto &imBackupRpc = imRpcMap["hunan-im"];
  if (imBackupRpc == nullptr) {
    spdlog::error("imBackupRpc is nullptr");
    return grpc::Status::CANCELLED;
  }
  auto rpcSuccess = imBackupRpc->ActiveService();
  if (!rpcSuccess) {
    return grpc::Status::CANCELLED;
  }
  response->set_msg("Pong!");
  return grpc::Status::OK;
}
}; // namespace wim::rpc