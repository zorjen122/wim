#pragma once
// ChatServiceManager.h
#include "ChatServer.h"
#include "Configer.h"
#include "Const.h"
#include "IocPool.h"
#include "Logger.h"
#include "RpcService.h"
#include "GatewayStreamService.h"
#include "GrpcSecurity.h"
#include "Service.h"

#include "Logger.h"
#include "Mysql.h"
#include "Redis.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <grpc/impl/codegen/grpc_types.h>
#include <atomic>
#include <cstddef>
#include <thread>
namespace wim {
class ImServiceRunner : public Singleton<ImServiceRunner> {
 private:
  void ClearUp() {
    if (cleaned.exchange(true))
      return;

    if (rpcServer)
      rpcServer->Shutdown();
    if (rpcRunThread.joinable()) {
      rpcRunThread.join();
      LOG_INFO(wim::businessLogger, "IM RPC服务线程退出成功");
    }

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
  }

 public:
  bool Activate() {
    try {
      auto config = Configer::getNode("server");
      unsigned short port = config["self"]["port"].as<int>();
      std::string host = config["self"]["host"].as<std::string>();
      std::string rpcPort = config["self"]["rpcPort"].as<std::string>();
      const bool legacyClientTcp = !config["self"]["legacyClientTcp"] ||
                                   config["self"]["legacyClientTcp"].as<bool>();
      const bool legacyPeerRpc = !config["self"]["legacyPeerRpc"] ||
                                 config["self"]["legacyPeerRpc"].as<bool>();
      std::string address = host + ":" + rpcPort;
      LOG_INFO(wim::netLogger,
               "IM网络服务器配置: host: {}, port: {}, rpcPort: {}", host, port,
               rpcPort);

      wim::IocPool::GetInstance();
      wim::db::MysqlDao::GetInstance();
      wim::db::RedisDao::GetInstance();

      // grpc服务
      rpc::ImRpcService service;
      auto transportSecurity = LoadGrpcSecurityConfig(config);
      rpc::GatewayStreamService gatewayStreamService(
          config["self"]["name"].as<std::string>(), transportSecurity.Mtls());
      Service::GetInstance()->SetGatewayStreamService(&gatewayStreamService);
      rpc::ServerBuilder builder;
      builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
      builder.AddListeningPort(address,
                               BuildServerCredentials(transportSecurity));
      if (legacyPeerRpc)
        builder.RegisterService(&service);
      builder.RegisterService(&gatewayStreamService);
      rpcServer = builder.BuildAndStart();
      LOG_INFO(wim::businessLogger, "IM RPC服务启动成功, 监听端口: {}",
               rpcPort);

      net::io_context acceptIoc;

      // 通讯服务
      if (legacyClientTcp) {
        imServer = std::make_unique<ChatServer>(acceptIoc, port);
        imServer->Start();
      } else {
        LOG_INFO(wim::businessLogger, "Message 节点客户端 TCP 回滚入口已关闭");
      }

      rpcRunThread = std::thread([&]() { rpcServer->Wait(); });

      boost::asio::signal_set signals(acceptIoc, SIGINT, SIGTERM);
      signals.async_wait(
          [&acceptIoc, &gatewayStreamService](
              const boost::system::error_code &error, int signalNumber) {
            if (error)
              return;
            gatewayStreamService.DrainAll("message node is shutting down");
            acceptIoc.stop();
          });

      acceptIoc.run();
      ClearUp();
      Service::GetInstance()->SetGatewayStreamService(nullptr);

      return true;
    } catch (const std::exception &e) {
      Service::GetInstance()->SetGatewayStreamService(nullptr);
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
  std::atomic<bool> cleaned{false};
};
};  // namespace wim
