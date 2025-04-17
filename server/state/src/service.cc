#include "service.h"
#include "Configer.h"
#include "global.h"
#include "im.pb.h"
#include "imRpc.h"
#include "spdlog/spdlog.h"
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <string>

StateServiceImpl::StateServiceImpl() {
  auto conf = Configer::getConfig("server");

  auto imTotal = conf["im"]["im-total"].as<int>();
  for (int index = 1; index <= imTotal; index++) {
    auto im = conf["im"]["s" + std::to_string(index)];
    auto host = im["host"].as<std::string>();
    auto port = im["rpcPort"].as<std::string>();
    auto name = im["name"].as<std::string>();
    auto status = im["status"].as<std::string>();
    ImNode::ptr node(new ImNode(host, port, status));
    spdlog::info("ImNode({}) {}:{} {} {}", std::to_string(index), host, port,
                 name, status);

    if (status == "backup" || status == "active") {
      if (status == "backup")
        imRpcMap[name] = std::make_unique<ImRpc>(node, 1);

      imNodeMap[name] = node;
      imNodeName.push_back(name);
    }
  }
}
grpc::Status StateServiceImpl::GetImServer(grpc::ServerContext *context,
                                           const ConnectUser *request,
                                           ConnectUserRsp *response) {
  static int routeCount = 0;

  int uid = request->id();

  int imTotal = imNodeName.size();
  for (int _ = 0; _ < imTotal; ++_) {
    auto nodeIndex = imNodeName[routeCount++];

    auto &node = imNodeMap[nodeIndex];
    routeCount = routeCount % imTotal;

    if (node->getStatus() == "active") {
      response->set_ip(node->getIp());
      response->set_port(atoi(node->getPort().c_str()));
      node->appendConnection(uid);
      break;
    }
  }

  return grpc::Status::OK;
}

grpc::Status
StateServiceImpl::ActiveImBackupServer(grpc::ServerContext *context,
                                       const ConnectUser *request,
                                       ConnectUserRsp *response) {
  static int routeCount = 0;

  int uid = request->id();
  int imTotal = imNodeName.size();
  bool onActive;

  for (int _ = 0; _ < imTotal; ++_) {
    auto nodeIndex = imNodeName[routeCount++];

    auto &node = imNodeMap[nodeIndex];
    routeCount = routeCount % imTotal;

    if (node->getStatus() == "backup") {
      auto &imBackupRpc = imRpcMap[nodeIndex];

      bool rpcSuccess = imBackupRpc->ActiveService();
      if (!rpcSuccess) {
        return grpc::Status::CANCELLED;
      }
      response->set_ip(node->getIp());
      response->set_port(atoi(node->getPort().c_str()));
      node->appendConnection(uid);
      node->setStatus("active");
      imRpcMap.erase(nodeIndex);
      onActive = true;
      break;
    }
  }

  return onActive ? grpc::Status::OK : grpc::Status::CANCELLED;
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