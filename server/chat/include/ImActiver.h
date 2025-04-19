#pragma once
// ChatServiceManager.h
#include "ChatServer.h"
#include "RpcPool.h"

#include "Configer.h"
#include "Const.h"
#include "IocPool.h"

#include "Mysql.h"
#include "Redis.h"
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <thread>
namespace wim {
// 若prc发送方发送合理，则无需互斥锁
class ImServiceRunner : public Singleton<ImServiceRunner> {
public:
  enum RunnerType { NORMAL_ACTIVE, BACKUP_ACTIVE };
  bool Activate(RunnerType type) {
    std::lock_guard<std::mutex> lock(activeMutex);
    if (isActive)
      return false;

    try {
      IocPool::GetInstance();
      wim::db::MysqlDao::GetInstance();
      wim::db::RedisDao::GetInstance();

      ioc = std::make_shared<net::io_context>();
      boost::asio::signal_set signals(*ioc, SIGINT, SIGTERM);
      signals.async_wait(
          [this](const boost::system::error_code &error, int signal_number) {
            if (error)
              return;
            ioc->stop();
          });

      auto config = Configer::getConfig("server");
      unsigned short port = config["self"]["port"].as<int>();

      imServer = std::make_shared<ChatServer>(*ioc, port);
      imServer->Start();
      isActive = true;

      if (type == BACKUP_ACTIVE) {
        runThread = std::thread([this] {
          ioc->run();
          spdlog::info("Chat service stopped");
        });
      } else if (type == NORMAL_ACTIVE) {
        ioc->run();
      }

      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to activate chat service: {}", e.what());
      return false;
    }
  }

  // 停止聊天服务
  void Deactivate() {
    // todo...
    isActive = false;
  }

  bool IsActive() const { return isActive; }
  ImServiceRunner() = default;
  ~ImServiceRunner() {
    if (runThread.joinable())
      runThread.join();
  }

private:
  std::shared_ptr<net::io_context> ioc;

  std::shared_ptr<ChatServer> imServer;
  std::thread runThread;
  bool isActive = false;
  mutable std::mutex activeMutex;
};
}; // namespace wim