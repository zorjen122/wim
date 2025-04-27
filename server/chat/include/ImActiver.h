#pragma once
// ChatServiceManager.h
#include "ChatServer.h"
#include "Configer.h"
#include "Const.h"
#include "ImRpc.h"
#include "IocPool.h"
#include "Logger.h"
#include "RpcService.h"

#include "Mysql.h"
#include "Redis.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>
#include <thread>
namespace wim {
// 若prc发送方发送合理，则无需互斥锁
class ImServiceRunner : public Singleton<ImServiceRunner> {
private:
  void PoolStop() {
    size_t useCount;
    useCount = IocPool::GetInstance().use_count();
    if (useCount > 2) {
      LOG_ERROR(wim::businessLogger, "IocPool资源池停止异常, useCount: {}",
                useCount);
    }
    IocPool::GetInstance()->Stop();

    useCount = db::MysqlDao::GetInstance().use_count();
    db::MysqlDao::GetInstance()->Close();
    if (useCount > 2)
      LOG_ERROR(wim::businessLogger, "MysqlDa资源池停止异常, useCount: {}",
                useCount);

    useCount = db::RedisDao::GetInstance().use_count();
    if (useCount > 2)
      LOG_ERROR(wim::businessLogger, "RedisDa资源池停止异常, useCount: {}",
                useCount);
    db::RedisDao::GetInstance()->Close();
  }

public:
  enum RunnerType { NORMAL_ACTIVE, BACKUP_ACTIVE };

  bool Activate(RunnerType type) {
    if (isActive)
      return true;

    try {
      auto config = Configer::getNode("server");
      unsigned short port = config["self"]["port"].as<int>();
      std::string host = config["self"]["host"].as<std::string>();
      std::string rpcPort = config["self"]["rpcPort"].as<std::string>();
      std::string address = host + ":" + rpcPort;
      LOG_INFO(wim::netLogger,
               "IM网络服务器配置: host: {}, port: {}, rpcPort: {}", host, port,
               rpcPort);

      wim::IocPool::GetInstance();
      wim::db::MysqlDao::GetInstance();
      wim::db::RedisDao::GetInstance();

      // grpc服务
      rpc::ImRpcService service;
      rpc::ServerBuilder builder;
      builder.AddListeningPort(address, grpc::InsecureServerCredentials());
      builder.RegisterService(&service);
      std::unique_ptr<grpc::Server> rpcServer(builder.BuildAndStart());
      LOG_INFO(wim::businessLogger, "IM RPC服务启动成功, 监听端口: {}",
               rpcPort);

      net::io_context &ioc = wim::IocPool::GetInstance()->GetContext();
      boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait(
          [this, &rpcServer](const boost::system::error_code &error,
                             int signalNumber) {
            if (error)
              return;
            rpcServer->Shutdown();
            PoolStop();

            LOG_INFO(wim::businessLogger,
                     "Received signal {}, stopping chat service: "
                     "[network、mysql、redis]",
                     signalNumber);
          });

      isActive = true;
      imServer = std::make_unique<ChatServer>(ioc, port);
      imServer->Start();

      rpcThread = std::thread([&]() { rpcServer->Wait(); });
      ioc.run();

      return true;
    } catch (const std::exception &e) {
      PoolStop();
      LOG_ERROR(businessLogger, "Failed to activate chat service: {}",
                e.what());
      return false;
    }
  }

  void Deactivate() { isActive = false; }

  bool IsActive() const { return isActive; }
  ImServiceRunner() = default;
  ~ImServiceRunner() {
    if (rpcThread.joinable())
      rpcThread.join();
  }

private:
  std::shared_ptr<net::io_context> ioc;
  std::shared_ptr<ChatServer> imServer;
  std::thread rpcThread;
  bool isActive = false;
}; // namespace wim
}; // namespace wim