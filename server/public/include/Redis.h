#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <memory>
#include <sw/redis++/redis.h>

#include <atomic>

#include "Configer.h"
#include "Const.h"
#include "Logger.h"
#include "spdlog/logger.h"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

namespace wim::db {

class RedisPool {
public:
  using Ptr = std::shared_ptr<RedisPool>;

  RedisPool(const std::string &host, unsigned int port,
            const std::string &password, size_t poolSize = 2)
      : host(host), port(port), stopEnable(false), password(password) {
    for (size_t i = 0; i < poolSize; ++i) {
      try {
        std::string url = "tcp://" + host + ":" + std::to_string(port);
        std::unique_ptr<sw::redis::Redis> session(new sw::redis::Redis(url));
        session->auth(password);
        connections.push(std::move(session));
      } catch (sw::redis::Error &e) {
        LOG_DEBUG(wim::dbLogger,
                  "redis connection is error: {}, continue this connection",
                  e.what());
        continue;
      }
    }

    if (connections.empty()) {
      spdlog::warn("REDIS CONNECTION POOL INIT IS WRONG, NO CONNECTION IS "
                   "SUCCESSFUL, EXIT NOW ");
      return;
    }

    keepThread = std::thread([this]() {
      short count = 0;
      while (!stopEnable) {
        if (count == 60) {
          keepConnectionHandle();
          count = 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        count++;
      }
    });
  }

  ~RedisPool() {
    Close();
    std::lock_guard<std::mutex> lock(poolMutex);
    while (!connections.empty()) {
      connections.pop();
    }

    // 等待时间不超1秒钟，因为keepThread线程函数每次休眠1秒
    if (keepThread.joinable())
      keepThread.join();
    LOG_DEBUG(dbLogger, "Redis Pool is destroyed!");
  }

  void ClearConnections() {
    std::lock_guard<std::mutex> lock(poolMutex);
    while (!connections.empty()) {
      auto context = std::move(connections.front());
      connections.pop();
    }
  }

  std::unique_ptr<sw::redis::Redis> GetConnection() {
    std::unique_lock<std::mutex> lock(poolMutex);
    condVar.wait(lock, [this] {
      if (stopEnable) {
        return true;
      }
      return !connections.empty();
    });
    if (stopEnable || connections.empty()) {
      return nullptr;
    }
    auto context = std::move(connections.front());

    connections.pop();
    return context;
  }

  void ReturnConnection(std::unique_ptr<sw::redis::Redis> context) {
    std::lock_guard<std::mutex> lock(poolMutex);
    if (stopEnable) {
      return;
    }
    connections.push(std::move(context));
    condVar.notify_one();
  }

  void Close() {
    stopEnable = true;
    condVar.notify_all();
  }

  bool Empty() {
    std::lock_guard<std::mutex> lock(poolMutex);
    return connections.empty();
  }

  size_t Size() {
    std::lock_guard<std::mutex> lock(poolMutex);
    return connections.size();
  }

private:
  void keepConnectionHandle() {
    std::lock_guard<std::mutex> lock(poolMutex);

    std::size_t len = connections.size();
    for (int i = 0; i < len; i++) {
      if (stopEnable)
        return;
      auto con = std::move(connections.front());
      connections.pop();
      try {
        auto v = con->get("PING");
        if (v->empty())
          throw sw::redis::Error("PING command is failed");

        connections.push(std::move(con));
      } catch (sw::redis::Error &e) {
        LOG_DEBUG(wim::dbLogger, "redis connection is error: {}, reconnect....",
                  e.what());
        con.reset();
        std::string url = "tcp://" + host + ":" + std::to_string(port);
        try {
          std::unique_ptr<sw::redis::Redis> session(new sw::redis::Redis(url));
          connections.push(std::move(session));
          session->auth(password);
        } catch (sw::redis::Error &e) {
          spdlog::warn("redis reconnection is error: {} | url: {}", e.what(),
                       url);
        }
      }
    }
  }

private:
  std::atomic<bool> stopEnable;
  std::string host;
  unsigned int port;
  std::string password;
  std::queue<std::unique_ptr<sw::redis::Redis>> connections;
  std::mutex poolMutex;
  std::condition_variable condVar;
  std::thread keepThread;
};

class RedisDao : public Singleton<RedisDao>,
                 std::enable_shared_from_this<RedisPool> {
public:
  using Ptr = std::shared_ptr<RedisDao>;
  RedisDao() {
    auto conf = Configer::getConfig("server");
    auto host = conf["redis"]["host"].as<std::string>();
    auto port = conf["redis"]["port"].as<int>();
    auto password = conf["redis"]["password"].as<std::string>();
    auto clientCount = conf["redis"]["clientCount"].as<int>();
    auto machine = conf["redis"]["machineId"].as<int>();

    redisPool.reset(new RedisPool(host, port, password, clientCount));
    machineId = machine;

    if (redisPool->Empty()) {
      dbLogger->warn("redis connection pool is empty, exit now");
    } else {
      dbLogger->info("redis connection pool init success, size: {}",
                     redisPool->Size());
    }
  }

  int64_t generateUserId() {
    static std::string __prefixUid = "im:uesrId";
    return generateId(__prefixUid);
  }

  int64_t generateMessageId() {
    static std::string __prefixUid = "im:msgId";
    return generateId(__prefixUid);
  }

  bool authVerifycode(const std::string &email, const std::string &verifycode) {
    auto redis = redisPool->GetConnection();
    if (!redis) {
      LOG_DEBUG(wim::dbLogger, "redis connection is null");
      return false;
    }
    Defer defer(
        [this, &redis]() { redisPool->ReturnConnection(std::move(redis)); });
    auto result = redis->get(email);
    return result.has_value() && result.value() == verifycode;
  }

private:
  int64_t generateId(const std::string &key) {

    auto redis = redisPool->GetConnection();
    if (redis == nullptr) {
      LOG_DEBUG(wim::dbLogger, "redis connection is null");
      return 1;
    }

    Defer defer(
        [this, &redis]() { redisPool->ReturnConnection(std::move(redis)); });
    // 1. 获取当前毫秒时间戳（41位）
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count() -
              epoch;

    // 2. 获取序列号（12位，每毫秒4096个）
    auto seq = redis->incr(key + ":" + std::to_string(ts));
    if (seq > 4095) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return generateId(key); // 递归直到不冲突
    }

    // 3. 组合ID：时间戳(41) + 机器ID(10) + 序列号(12)
    return (ts << 22) | (machineId << 12) | seq;
  }

private:
  RedisPool::Ptr redisPool;
  uint16_t machineId; // 集群中每个节点的唯一ID (0-1023)
  static const int64_t epoch = 1672531200000; // 2023-01-01 00:00:00
};
}; // namespace wim::db