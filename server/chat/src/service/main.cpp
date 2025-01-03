#include <boost/asio/io_context.hpp>
#include <csignal>
#include <memory>
#include <thread>

#include "ChatServer.h"
#include "ChatSession.h"
#include "ConfigManager.h"
#include "IOServicePool.h"
#include "MysqlManager.h"
#include "RedisManager.h"
#include "RpcChatService.h"

void poolInit() {
  RedisManager::GetInstance();
  MysqlManager::GetInstance();
  // ServicePool::GetInstance();
}

std::unique_ptr<grpc::Server> rpcStartServer(const std::string &rpcAddress) {
  ChatServiceImpl service;
  grpc::ServerBuilder builder;

  // 监听端口和添加服务
  builder.AddListeningPort(rpcAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  // 构建并启动gRPC服务器
  std::unique_ptr<grpc::Server> rpcServer(builder.BuildAndStart());
  std::cout << "RPC Server listening on " << rpcAddress << "\n";

  return rpcServer;
}

int main() {
  auto existConfig = ConfigManager::loadConfig("../config.yaml");
  if (!existConfig) {
    spdlog::error("Config load failed");
    return 0;
  }

  auto config = ConfigManager::getConfig("Server");
  if(!config || !config["Self"])
  {
    spdlog::error("Self config not found");
    return 0;
  }
  std::string rpcAddress =
      config["Self"]["Host"].as<std::string>() 
      + ":" 
      + config["Self"]["RpcPort"].as<std::string>();
    std::string server_name = config["Self"]["Name"].as<std::string>();

    spdlog::info("Server started on name: {}", server_name);
    spdlog::info("Server started on host: {}", config["Self"]["Host"].as<std::string>());
    spdlog::info("Server started on port: {}", config["Self"]["Port"].as<std::string>());
    spdlog::info("Server started on rpc port: {}", config["Self"]["RpcPort"].as<std::string>());

  try {
    poolInit();

    auto rpcServer = rpcStartServer(rpcAddress);
    auto server_pool = ServicePool::GetInstance();

    boost::asio::io_context ioContext;
    boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait([&ioContext, &server_pool, &rpcServer](auto, auto) {
      ioContext.stop();
      server_pool->Stop();
      rpcServer->Shutdown();
    });

    std::thread rpcThread([&rpcServer]() { rpcServer->Wait(); });

    auto port = config["Self"]["Port"].as<std::string>();
    std::shared_ptr<ChatServer> server(
        new ChatServer(ioContext, atoi(port.c_str())));

    server->Start();
    ioContext.run();

    rpcThread.join();
    RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
                                      server_name);
    RedisManager::GetInstance()->Close();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << endl;
    RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
                                      server_name);
    RedisManager::GetInstance()->Close();
  }

  return 0;
}
