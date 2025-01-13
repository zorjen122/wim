#pragma once
#include "hiredis.h"

#include "Const.h"
#include <atomic>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

class RedisPool {
public:
  RedisPool(size_t poolSize, const std::string &host, unsigned int port,
            const std::string &pwd)
      : poolSize_(poolSize), host_(host), port_(port), _isStop(false),
        passwd_(pwd), counter_(0) {
    for (size_t i = 0; i < poolSize_; ++i) {
      auto *context = redisConnect(host.c_str(), port);
      if (context == nullptr || context->err != 0) {
        if (context != nullptr) {
          redisFree(context);
        }
        continue;
      }

      auto reply = (redisReply *)redisCommand(context, "AUTH %s", pwd.c_str());
      if (reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(reply);
        continue;
      }

      freeReplyObject(reply);

      connections_.push(context);
    }

    check_thread_ = std::thread([this]() {
      while (!_isStop) {
        counter_++;
        if (counter_ >= 60) {
          checkThread();
          counter_ = 0;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });
  }

  ~RedisPool() {}

  void ClearConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) {
      auto *context = connections_.front();
      redisFree(context);
      connections_.pop();
    }
  }

  redisContext *GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] {
      if (_isStop) {
        return true;
      }
      return !connections_.empty();
    });
    if (_isStop) {
      return nullptr;
    }
    auto *context = connections_.front();
    connections_.pop();
    return context;
  }

  void ReturnConnection(redisContext *context) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (_isStop) {
      return;
    }
    connections_.push(context);
    cond_.notify_one();
  }

  void Close() {
    _isStop = true;
    cond_.notify_all();
    check_thread_.join();
  }

private:
  void checkThread() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (_isStop) {
      return;
    }
    auto pool_size = connections_.size();
    for (int i = 0; i < pool_size && !_isStop; i++) {
      auto *context = connections_.front();
      connections_.pop();
      try {
        auto reply = (redisReply *)redisCommand(context, "PING");
        if (!reply) {
          connections_.push(context);
          continue;
        }
        freeReplyObject(reply);
        connections_.push(context);
      } catch (std::exception &exp) {

        spdlog::error("redis connection is error: {}", exp.what());
        redisFree(context);
        context = redisConnect(host_.c_str(), port_);
        if (context == nullptr || context->err != 0) {
          if (context != nullptr) {
            redisFree(context);
          }
          continue;
        }

        auto reply =
            (redisReply *)redisCommand(context, "AUTH %s", passwd_.c_str());
        if (reply->type == REDIS_REPLY_ERROR) {
          freeReplyObject(reply);
          continue;
        }

        freeReplyObject(reply);
        connections_.push(context);
      }
    }
  }
  std::atomic<bool> _isStop;
  size_t poolSize_;
  std::string host_;
  std::string passwd_;
  int port_;
  std::queue<redisContext *> connections_;
  std::mutex mutex_;
  std::condition_variable cond_;
  std::thread check_thread_;
  int counter_;
};

class RedisManager : public Singleton<RedisManager>,
                     public std::enable_shared_from_this<RedisManager> {
  friend class Singleton<RedisManager>;

public:
  ~RedisManager();
  bool Get(const std::string &key, std::string &value);
  bool Set(const std::string &key, const std::string &value);
  bool LPush(const std::string &key, const std::string &value);
  bool LPop(const std::string &key, std::string &value);
  bool RPush(const std::string &key, const std::string &value);
  bool RPop(const std::string &key, std::string &value);
  bool HSet(const std::string &key, const std::string &hkey,
            const std::string &value);
  bool HSet(const char *key, const char *hkey, const char *hvalue,
            size_t hvaluelen);
  std::string HGet(const std::string &key, const std::string &hkey);
  bool HDel(const std::string &key, const std::string &field);
  bool Del(const std::string &key);
  bool ExistsKey(const std::string &key);
  void Close();

private:
  RedisManager();
  std::unique_ptr<RedisPool> pool;
};
