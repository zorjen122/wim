#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <memory>
#include <sw/redis++/redis.h>

#include <atomic>

#include "Configer.h"
#include "Const.h"
#include "DbGlobal.h"
#include "Logger.h"
#include <condition_variable>
#include <jsoncpp/json/json.h>
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
    LOG_INFO(dbLogger, "RedisPool::~RedisPool()!");
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
  friend class TestDb;

public:
  using Ptr = std::shared_ptr<RedisDao>;
  RedisDao() {
    static bool inited = false;
    if (inited) {
      LOG_WARN(dbLogger, "RedisDao is already inited");
      return;
    }
    auto conf = Configer::getNode("server");
    auto host = conf["redis"]["host"].as<std::string>();
    auto port = conf["redis"]["port"].as<int>();
    auto password = conf["redis"]["password"].as<std::string>();
    auto clientCount = conf["redis"]["clientCount"].as<int>();
    auto machine = conf["redis"]["machineId"].as<int>();

    redisPool.reset(new RedisPool(host, port, password, clientCount));
    machineId = machine;

    if (!redisPool->Empty()) {
      LOG_INFO(dbLogger,
               "redis connection pool init success | host: {}, port: "
               "{}, password: {}, clientCount: {}, machineId: {}",
               host, port, password, clientCount, machine);
    } else {
      LOG_WARN(dbLogger,
               "redis connection pool init failed | host: {}, port: "
               "{}, password: {}, clientCount: {}, machineId: {}",
               host, port, password, clientCount, machine);
    }
    inited = true;
  }

  /*
  接口说明：【2025-5-2】
    对于int、long返回类型，返回-1表示存储时出错
    对于指针（shared_ptr）返回类型，错误时统一返回nullptr
    对于bool返回类型，false表示错误
    返回接口定义处见：defaultForType
  */
  template <typename Func>
  auto executeTemplate(Func &&processor) -> decltype(
      processor(std::declval<std::unique_ptr<sw::redis::Redis> &>())) {
    auto con = redisPool->GetConnection();
    if (con == nullptr) {
      LOG_INFO(dbLogger, "Redis connection pool is empty!");
      return defaultForType<decltype(processor(con))>();
    }
    Defer defer(
        [&con, this]() { redisPool->ReturnConnection(std::move(con)); });

    try {
      return processor(con);
    } catch (sw::redis::Error &e) {
      LOG_TRACE(dbLogger, "MySQL error: {}", e.what());
      return defaultForType<decltype(processor(con))>();
    }
  }
  template <typename T> static auto defaultForType() {
    if constexpr (std::is_same_v<T, bool>) {
      return false;
    } else if constexpr (std::is_integral_v<T>) {
      return -1;
    } else {
      return T{};
    }
  }

  void Close() { return redisPool->Close(); }

  const std::string PrefixUserId = "im:userId:";
  std::string getPrefixUserId() { return PrefixUserId; }

  const std::string PrefixOnlineUserInfo = "im:user:";
  std::string getPrefixOnlineUserInfo() { return PrefixOnlineUserInfo; }

  const std::string __prefixUid = "im:userId";
  int64_t generateUserId() { return generateId(__prefixUid); }

  const std::string __prefixMsgId = "im:msgId";
  int64_t generateMsgId() { return generateId(__prefixMsgId); }

  const std::string __prefixSessionId = "im:sessionId";
  int64_t generateSessionId() { return generateId(__prefixSessionId); }

  const std::string __prefixGid = "im:gid";
  int64_t generateGid() { return generateId(__prefixGid); }

  std::string getMsgId(int64_t msgId) {
    return executeTemplate(
        [&](std::unique_ptr<sw::redis::Redis> &redis) -> std::string {
          auto result = redis->get(__prefixMsgId + ":" + std::to_string(msgId));

          return *result;
        });
  }

  const std::string __prefixUserMsgId = "im:userMsgId";
  bool setUserMsgId(long userId, int64_t msgId, short expired) {
    return executeTemplate([&](std::unique_ptr<sw::redis::Redis> &redis) {
      redis->setex(__prefixUserMsgId + ":" + std::to_string(userId) + ":" +
                       std::to_string(msgId),
                   expired, "1");
      return true;
    });
  }

  bool getUserMsgId(long userId, int64_t msgId) {
    return executeTemplate([&](std::unique_ptr<sw::redis::Redis> &redis) {
      auto result =
          redis->get(__prefixUserMsgId + ":" + std::to_string(userId) + ":" +
                     std::to_string(msgId));
      return result.has_value();
    });
  }

  bool expireUserMsgId(long userId, int64_t msgId, short expired) {
    return executeTemplate([&](std::unique_ptr<sw::redis::Redis> &redis) {
      auto result =
          redis->expire(__prefixUserMsgId + ":" + std::to_string(userId) + ":" +
                            std::to_string(msgId),
                        expired);
      return result;
    });
  }

  bool authVerifycode(const std::string &email, const std::string &verifycode) {
    return executeTemplate([&](std::unique_ptr<sw::redis::Redis> &redis) {
      auto result = redis->get(email);

      return result.has_value() && result.value() == verifycode;
    });
  }

  // 暂用字符串MachineId标识机器
  bool setOnlineUserInfo(db::UserInfo::Ptr userInfo, std::string machineId) {
    return executeTemplate([&](std::unique_ptr<sw::redis::Redis> &redis) {
      Json::Value jsonData;
      jsonData["userId"] = Json::Value::Int64(userInfo->uid);
      jsonData["name"] = userInfo->name;
      jsonData["age"] = userInfo->age;
      jsonData["sex"] = userInfo->sex;
      jsonData["headImageURL"] = userInfo->headImageURL;
      jsonData["machineId"] = machineId;
      bool status =
          redis->set(getPrefixOnlineUserInfo() + std::to_string(userInfo->uid),
                     jsonData.toStyledString());
      return status;
    });
  }

  std::string getOnlineUserInfo(long uid) {
    auto redis = redisPool->GetConnection();
    if (!redis) {
      LOG_DEBUG(wim::dbLogger, "redis connection is null");
    }
    Defer defer(
        [this, &redis]() { redisPool->ReturnConnection(std::move(redis)); });

    auto source = redis->get(getPrefixOnlineUserInfo() + std::to_string(uid));
    if (!source.has_value())
      return "";

    return source.value();
  }

  Json::Value getOnlineUserInfoObject(long uid) {
    std::string source = getOnlineUserInfo(uid);
    if (source.empty())
      return {};
    Json::Reader reader;
    Json::Value jsonData;
    if (!reader.parse(source, jsonData)) {
      LOG_DEBUG(wim::dbLogger, "parse online user info json data error");
      return {};
    }
    return jsonData;
  }

  bool delOnlineUserInfo(long uid) {
    return executeTemplate([&](std::unique_ptr<sw::redis::Redis> &redis) {
      redis->del(getPrefixOnlineUserInfo() + std::to_string(uid));
      return true;
    });
  }

private:
  int64_t generateId(const std::string &key) {
    return executeTemplate(
        [&](std::unique_ptr<sw::redis::Redis> &redis) -> int64_t {
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
        });
  }

private:
  RedisPool::Ptr redisPool;
  uint16_t machineId; // 集群中每个节点的唯一ID (0-1023)
  static const int64_t epoch = 1672531200000; // 2023-01-01 00:00:00
};

}; // namespace wim::db