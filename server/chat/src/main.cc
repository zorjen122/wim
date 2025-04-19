#include "RpcService.h"

#include "ImActiver.h"
#include "Logger.h"
#include <string>
namespace wim {
void ImBackupServiceRunner() {

  auto conf = Configer::getConfig("server");

  auto host = conf["self"]["host"].as<std::string>();
  auto rpcPort = conf["self"]["rpcPort"].as<std::string>();
  auto address = host + ":" + rpcPort;

  rpc::ImRpcService service;
  rpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  LOG_INFO(wim::netLogger, "ImBackupServiceRunner started on {}", address);

  server->Wait();
  LOG_INFO(wim::netLogger, "ImBackupServiceRunner stopped");
}
} // namespace wim

int main(int argc, char *argv[]) {

  if (argc == 3) {
    auto existConfig = Configer::loadConfig(argv[1]);
    if (!existConfig) {
      spdlog::error("Config load failed");
      return 0;
    }

    auto config = Configer::getConfig("server");
    if (!config || !config["self"]) {
      spdlog::error("self config not found");
      return 0;
    }
    auto rpcAddress = config["self"]["host"].as<std::string>() + ":" +
                      config["self"]["rpcPort"].as<std::string>();

    spdlog::info("Server started on name: {}",
                 config["self"]["name"].as<std::string>());
    spdlog::info("Server started on host: {}",
                 config["self"]["host"].as<std::string>());
    spdlog::info("Server started on port: {}",
                 config["self"]["port"].as<std::string>());
    spdlog::info("Server started on rpc port: {}",
                 config["self"]["rpcPort"].as<std::string>());

    if (argc == 3 && std::string(argv[2]) == "--normal") {
      wim::ImServiceRunner::GetInstance()->Activate(
          wim::ImServiceRunner::NORMAL_ACTIVE);
    } else {
      wim::ImBackupServiceRunner();
    }

    return 0;
  }
}
