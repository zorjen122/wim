#include <boost/asio/io_context.hpp>
#include <csignal>
#include <memory>
#include <mutex>
#include <thread>

#include "ChatServer.h"
#include "ChatSession.h"
#include "Configer.h"
#include "IOServicePool.h"
#include "MysqlManager.h"
#include "RedisManager.h"
#include "RpcChatService.h"

void poolInit() {
  RedisManager::GetInstance();
  MysqlManager::GetInstance();
  // IOServicePool::GetInstance();
}

std::unique_ptr<grpc::Server> rpcStartServer(const std::string &rpcAddress) {
  ChatServiceImpl service;
  grpc::ServerBuilder builder;

  // 监听端口和添加服务
  builder.AddListeningPort(rpcAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  // 构建并启动gRPC服务器
  std::unique_ptr<grpc::Server> rpc_server(builder.BuildAndStart());
  std::cout << "RPC Server listening on " << rpcAddress << "\n";

  return rpc_server;
}

int main() {
  auto &config = Configer::GetInstance();
  std::string rpc_address =
      config["SelfServer"]["Host"] + ":" + config["SelfServer"]["RPCPort"];
  auto server_name = config["SelfServer"]["Name"];

  try {
    poolInit();

    auto rpc_server = rpcStartServer(rpc_address);
    auto server_pool = IOServicePool::GetInstance();

    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context, &server_pool, &rpc_server](auto, auto) {
      io_context.stop();
      server_pool->Stop();
      rpc_server->Shutdown();
    });

    std::thread rpc_thread([&rpc_server]() { rpc_server->Wait(); });

    auto port = config["SelfServer"]["Port"];
    std::shared_ptr<ChatServer> server(
        new ChatServer(io_context, atoi(port.c_str())));

    server->Start();

    io_context.run();

    rpc_thread.join();
    RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
                                      server_name);
    RedisManager::GetInstance()->Close();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << endl;
    RedisManager::GetInstance()->HDel(PREFIX_REDIS_USER_ACTIVE_COUNT,
                                      server_name);
    RedisManager::GetInstance()->Close();
  }
}
