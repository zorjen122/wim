#include "Configer.h"
#include "service.h"
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>
/*
 * 注意：该状态服务只是一个微服务，运行在网关服务器主机，其目的仅在后续扩展以及独立网关服务器的路由职责。[2025-3-18]
 */
int main() {

  auto existConfig = Configer::loadConfig("../config.yaml");
  if (!existConfig) {
    spdlog::error("Config load failed");
    return 0;
  }

  auto config = Configer::getConfig("server");
  if (!config || !config["state"]) {
    spdlog::error("self config not found");
    return 0;
  }

  auto host = config["state"]["host"].as<std::string>();
  auto port = config["state"]["port"].as<std::string>();

  StateServiceImpl stateImpl;
  grpc::ServerBuilder builder;
  builder.AddListeningPort(host + ":" + port,
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&stateImpl);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  spdlog::info("State Service on startting, listening on {}:{}");
  server->Wait();

  return 0;
}