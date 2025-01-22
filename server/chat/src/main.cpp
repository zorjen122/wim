#include <boost/asio/io_context.hpp>
#include <csignal>
#include <memory>
#include <thread>

#include "ChatServer.h"
#include "ChatSession.h"
#include "Configer.h"
#include "IocPool.h"
#include "RedisManager.h"
#include "RpcServer.h"
#include "SqlOperator.h"

#include <spdlog/spdlog.h>

std::unique_ptr<grpc::Server> rpcStartServer(const std::string &rpcAddress) {
  // ChatServiceImpl service;
  grpc::ServerBuilder builder;

  // 监听端口和添加服务
  builder.AddListeningPort(rpcAddress, grpc::InsecureServerCredentials());
  // builder.RegisterService(&service);

  // 构建并启动gRPC服务器
  std::unique_ptr<grpc::Server> rpcServer(builder.BuildAndStart());
  spdlog::info("RPC Server starting on " + rpcAddress);
  return rpcServer;
}

int main() {
  auto existConfig = Configer::loadConfig("../config.yaml");
  if (!existConfig) {
    spdlog::error("Config load failed");
    return 0;
  }

  auto config = Configer::getConfig("server");
  if (!config || !config["self"]) {
    spdlog::error("self config not found");
    return 0;
  }
  std::string rpcAddress = config["self"]["host"].as<std::string>() + ":" +
                           config["self"]["rpcPort"].as<std::string>();
  std::string server_name = config["self"]["name"].as<std::string>();

  spdlog::info("Server started on name: {}", server_name);
  spdlog::info("Server started on host: {}",
               config["self"]["host"].as<std::string>());
  spdlog::info("Server started on port: {}",
               config["self"]["port"].as<std::string>());
  spdlog::info("Server started on rpc port: {}",
               config["self"]["rpcPort"].as<std::string>());

  try {
    RedisManager::GetInstance();
    MySqlOperator::GetInstance();
    net::io_context ioc;
    auto serverPool = IocPool::GetInstance();

    auto port = config["self"]["port"].as<std::string>();
    std::shared_ptr<ChatServer> server(new ChatServer(ioc, atoi(port.c_str())));
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc, &serverPool](auto, auto) {
      ioc.stop();
      serverPool->Stop();
    });
    server->Start();
    ioc.run();
  } catch (std::exception &e) {
    spdlog::error("Exception: {}", e.what());
    RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
                                      server_name);
    RedisManager::GetInstance()->Close();
  }

  // try {
  //   RedisManager::GetInstance();
  //   MysqlManager::GetInstance();

  //   auto rpcServer = rpcStartServer(rpcAddress);
  //   auto serverPool = IocPool::GetInstance();

  //   boost::asio::io_context ioContext;
  //   boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
  //   signals.async_wait([&ioContext, &serverPool, &rpcServer](auto, auto) {
  //     ioContext.stop();
  //     serverPool->Stop();
  //     rpcServer->Shutdown();
  //   });

  //   std::thread rpcThread([&rpcServer]() { rpcServer->Wait(); });

  //   auto port = config["self"]["port"].as<std::string>();
  //   std::shared_ptr<ChatServer> server(
  //       new ChatServer(ioContext, atoi(port.c_str())));

  //   server->Start();
  //   ioContext.run();

  //   rpcThread.join();
  //   RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
  //                                     server_name);
  //   RedisManager::GetInstance()->Close();
  // } catch (std::exception &e) {
  //   spdlog::error("Exception: {}", e.what());
  //   RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
  //                                     server_name);
  //   RedisManager::GetInstance()->Close();
  // }

  return 0;
}
