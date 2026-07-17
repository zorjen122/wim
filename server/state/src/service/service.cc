#include "service.h"
#include "Configer.h"
#include "global.h"
#include "im.pb.h"
#include "imRpc.h"
#include "spdlog/spdlog.h"
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <algorithm>
#include <cstdint>
#include <string>

namespace wim::rpc {
namespace {

std::vector<ServiceNodeInfo> LoadNodes(const YAML::Node &server,
                                       const std::string &sectionName,
                                       const std::string &prefix,
                                       const std::string &totalKey,
                                       const std::string &portKey) {
  std::vector<ServiceNodeInfo> nodes;
  auto section = server[sectionName];
  if (!section || !section[totalKey])
    return nodes;

  auto total = section[totalKey].as<int>();
  nodes.reserve(std::max(total, 0));
  for (int i = 1; i <= total; ++i) {
    auto source = section[prefix + std::to_string(i)];
    if (!source)
      continue;

    ServiceNodeInfo node;
    node.id = source["name"].as<std::string>();
    node.host = source["host"].as<std::string>();
    node.port = source[portKey].as<unsigned short>();
    node.status = source["status"] ? source["status"].as<std::string>()
                                   : std::string{"active"};
    node.weight = source["weight"] ? source["weight"].as<unsigned int>() : 1;
    nodes.push_back(std::move(node));
  }
  return nodes;
}

}  // namespace

StateServiceImpl::StateServiceImpl() {
  auto conf = Configer::getNode("server");

  gatewayNodes =
      LoadNodes(conf, "connection-gateway", "g", "gateway-total", "port");
  messageNodes = LoadNodes(conf, "message", "m", "message-total", "streamPort");
  if (conf["topology-version"])
    topologyVersion = conf["topology-version"].as<std::uint64_t>();

  auto imTotal = conf["im"] && conf["im"]["im-total"]
                     ? conf["im"]["im-total"].as<int>()
                     : 0;
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

  // 兼容旧配置：尚未配置独立 Gateway/Message 时仍可启动 State。
  if (gatewayNodes.empty()) {
    gatewayNodes = LoadNodes(conf, "im", "s", "im-total", "port");
  }
  if (messageNodes.empty()) {
    messageNodes = LoadNodes(conf, "im", "s", "im-total", "rpcPort");
  }
}

grpc::Status StateServiceImpl::PickConnectionGateway(grpc::ServerContext *,
                                                     const ConnectUser *,
                                                     ConnectUserRsp *response) {
  if (gatewayNodes.empty())
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "no connection gateway configured");

  const std::size_t start =
      gatewayRouteCount.fetch_add(1, std::memory_order_relaxed);
  for (std::size_t offset = 0; offset < gatewayNodes.size(); ++offset) {
    const auto &node = gatewayNodes[(start + offset) % gatewayNodes.size()];
    if (!node.active())
      continue;
    response->set_ip(node.host);
    response->set_port(node.port);
    response->set_node_id(node.id);
    return grpc::Status::OK;
  }

  return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                      "no active connection gateway");
}

grpc::Status StateServiceImpl::ListMessageNodes(grpc::ServerContext *,
                                                const TopologyRequest *request,
                                                MessageTopology *response) {
  response->set_topology_version(topologyVersion);
  if (request->known_version() == topologyVersion)
    return grpc::Status::OK;

  for (const auto &node : messageNodes) {
    if (!node.active())
      continue;
    auto *target = response->add_nodes();
    target->set_node_id(node.id);
    target->set_host(node.host);
    target->set_port(node.port);
    target->set_status(node.status);
    target->set_weight(node.weight);
  }
  return grpc::Status::OK;
}
grpc::Status StateServiceImpl::GetImServer(grpc::ServerContext *context,
                                           const ConnectUser *request,
                                           ConnectUserRsp *response) {
  static int routeCount = 0;

  int uid = request->id();

  int imTotal = imNodeName.size();
  if (imTotal == 0)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "legacy IM routing is disabled");
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

grpc::Status StateServiceImpl::ActiveImBackupServer(
    grpc::ServerContext *context, const ConnectUser *request,
    ConnectUserRsp *response) {
  static int routeCount = 0;

  int uid = request->id();
  int imTotal = imNodeName.size();
  if (imTotal == 0)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "legacy IM routing is disabled");
  bool onActive = false;

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
};  // namespace wim::rpc
