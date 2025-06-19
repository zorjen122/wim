#pragma once
// ChatServiceManager.h
#include "ChatServer.h"
#include "Configer.h"
#include "Const.h"
#include "IocPool.h"
#include "Logger.h"
#include "RpcService.h"

#include "Logger.h"
#include "Mysql.h"
#include "Redis.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <thread>
namespace wim {
class ImServiceRunner : public Singleton<ImServiceRunner> {
private:
  void ClearUp() {
    size_t refCount;
    // 减去instance实例拷贝的一份引用
    refCount = IocPool::GetInstance().use_count() - 1;
    if (refCount > 1) {
      LOG_ERROR(wim::businessLogger, "IocPool资源池状态异常, 引用计数: {}",
                refCount);
    }
    IocPool::GetInstance()->Stop();

    refCount = db::MysqlDao::GetInstance().use_count() - 1;
    db::MysqlDao::GetInstance()->Close();
    if (refCount > 1)
      LOG_ERROR(wim::dbLogger, "MysqlDao资源池状态异常, 引用计数", refCount);

    refCount = db::RedisDao::GetInstance().use_count() - 1;
    if (refCount > 1)
      LOG_ERROR(wim::dbLogger, "RedisDao资源池状态异常, 引用计数", refCount);
    db::RedisDao::GetInstance()->Close();

    rpcServer->Shutdown();
    if (rpcRunThread.joinable()) {
      rpcRunThread.join();
      LOG_INFO(wim::businessLogger, "IM RPC服务线程退出成功");
    }
  }

public:
  bool Activate() {
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
      rpcServer = builder.BuildAndStart();
      LOG_INFO(wim::businessLogger, "IM RPC服务启动成功, 监听端口: {}",
               rpcPort);

      io_context &ioc = wim::IocPool::GetInstance()->GetContext();

      // 通讯服务
      imServer = std::make_unique<ChatServer>(ioc, port);
      imServer->Start();

      rpcRunThread = std::thread([&]() { rpcServer->Wait(); });

      boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait(
          [this](const boost::system::error_code &error, int signalNumber) {
            if (error)
              return;
            ClearUp();
          });

      ioc.run();

      return true;
    } catch (const std::exception &e) {
      ClearUp();
      LOG_ERROR(businessLogger, "通讯服务启动失败, 错误信息: {}", e.what());
      return false;
    }
  }

  ImServiceRunner() = default;
  ~ImServiceRunner() {
    ClearUp();
    LOG_INFO(businessLogger, "ImServiceRunner::~ImServiceRunner");
  }

private:
  std::unique_ptr<grpc::Server> rpcServer;
  std::shared_ptr<ChatServer> imServer;
  std::thread rpcRunThread;
};
}; // namespace wim