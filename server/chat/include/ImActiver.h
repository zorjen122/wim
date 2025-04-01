#pragma once
// ChatServiceManager.h
#include "ChatServer.h"
#include "Configer.h"
#include "Const.h"
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <thread>

// 若prc发送方发送合理，则无需互斥锁
class ImServiceRunner : public Singleton<ImServiceRunner> {
public:
  enum RunnerType { NORMAL_RUN, BACKUP_RUN };
  bool Activate() {
    std::lock_guard<std::mutex> lock(activeMutex);
    if (isActive)
      return false;

    try {
      // 初始化资源（Redis、MySQL等）
      // RedisManager::GetInstance();
      // MySqlOperator::GetInstance();

      // 创建独立的 io_context 和线程
      ioc = std::make_shared<net::io_context>();
      worker = std::make_shared<
          net::executor_work_guard<net::io_context::executor_type>>(
          ioc->get_executor());

      // 启动聊天服务器
      auto config = Configer::getConfig("server");
      if (!config || !config["self"]) {
        spdlog::error("self config not found");
        return false;
      }
      int port = config["self"]["port"].as<int>();
      imServer = std::make_shared<ChatServer>(*ioc, port);
      imServer->Start();

      // 启动独立线程运行 io_context
      runThread = std::thread([this] {
        ioc->run();
        spdlog::info("Chat service stopped");
      });

      isActive = true;
      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to activate chat service: {}", e.what());
      return false;
    }
  }

  // 停止聊天服务
  void Deactivate() {
    std::lock_guard<std::mutex> lock(activeMutex);
    if (!isActive)
      return;

    // 停止 io_context
    if (ioc) {
      ioc->stop();
      if (runThread.joinable()) {
        runThread.join();
      }
      worker.reset();
      ioc.reset();
    }
    // 清理资源
    // RedisOperator::GetInstance()->Close();
    imServer.reset();
    isActive = false;
  }

  bool IsActive() const { return isActive; }
  ImServiceRunner() = default;
  ~ImServiceRunner() { Deactivate(); }

private:
  std::shared_ptr<net::io_context> ioc;
  std::shared_ptr<net::executor_work_guard<net::io_context::executor_type>>
      worker;
  std::shared_ptr<ChatServer> imServer;
  std::thread runThread;
  bool isActive = false;
  mutable std::mutex activeMutex;
};
