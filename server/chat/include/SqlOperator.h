#pragma once
#include <condition_variable>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

#include "Const.h"
#include "Package.h"
#include "spdlog/spdlog.h"

class SqlConnection {
public:
  SqlConnection(sql::Connection *connection, int64_t lastTime)
      : sql(connection), lastOperTime(lastTime) {}
  std::unique_ptr<sql::Connection> sql;
  int64_t lastOperTime;
};

class MySqlPool {
public:
  MySqlPool(const std::string &url, const std::string &user,
            const std::string &passwd, const std::string &schema,
            std::size_t poolSize)
      : url_(url), user_(user), passwd_(passwd), schema_(schema),
        poolSize_(poolSize), b_stop_(false) {
    try {
      for (int i = 0; i < poolSize_; ++i) {
        auto *driver = sql::mysql::get_mysql_driver_instance();
        auto *con = driver->connect(url_, user_, passwd_);
        con->setSchema(schema_);

        if (!con->isValid())
          continue;

        auto currentTime = std::chrono::system_clock::now().time_since_epoch();
        long long timestamp =
            std::chrono::duration_cast<std::chrono::seconds>(currentTime)
                .count();
        pool_.push(std::make_unique<SqlConnection>(con, timestamp));

        spdlog::info("Mysql-client init ok!");
      }
      _check_thread = std::thread([this]() {
        while (!b_stop_) {
          checkConnection();
          std::this_thread::sleep_for(std::chrono::seconds(60));
        }
      });

      _check_thread.detach();
    } catch (sql::SQLException &e) {
      // 处理异常
      spdlog::error("mysql pool init failed, error is {}", e.what());
    }
  }

  void checkConnection() {
    std::lock_guard<std::mutex> lock(mutex_);
    int poolsize = pool_.size();
    // 获取当前时间戳
    auto currentTime = std::chrono::system_clock::now().time_since_epoch();
    // 将时间戳转换为秒
    long long timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
    for (int i = 0; i < poolsize; i++) {
      auto con = std::move(pool_.front());
      pool_.pop();
      Defer _([this, &con]() { pool_.push(std::move(con)); });

      if (timestamp - con->lastOperTime < 5) {
        continue;
      }

      try {
        std::unique_ptr<sql::Statement> stmt(con->sql->createStatement());
        stmt->executeQuery("SELECT 1");
        con->lastOperTime = timestamp;
      } catch (sql::SQLException &e) {
        std::cout << "Error keeping connection alive: " << e.what()
                  << std::endl;
        // 重新创建连接并替换旧的连接
        auto *driver = sql::mysql::get_mysql_driver_instance();
        auto *newcon = driver->connect(url_, user_, passwd_);
        newcon->setSchema(schema_);
        con->sql.reset(newcon);
        con->lastOperTime = timestamp;
      }
    }
  }

  std::unique_ptr<SqlConnection> getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] {
      if (b_stop_) {
        return true;
      }
      return !pool_.empty();
    });
    if (b_stop_) {
      return nullptr;
    }
    std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
    pool_.pop();
    return con;
  }

  void returnConnection(std::unique_ptr<SqlConnection> con) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (b_stop_) {
      return;
    }
    pool_.push(std::move(con));
    cond_.notify_one();
  }

  void close() {
    b_stop_ = true;
    cond_.notify_all();
  }

  ~MySqlPool() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
      pool_.pop();
    }
  }

private:
  std::string url_;
  std::string user_;
  std::string passwd_;
  std::string schema_;
  int poolSize_;
  std::queue<std::unique_ptr<SqlConnection>> pool_;
  std::mutex mutex_;
  std::condition_variable cond_;
  std::atomic<bool> b_stop_;
  std::thread _check_thread;
};

class MySqlOperator : public Singleton<MySqlOperator> {
  friend class Singleton<MySqlOperator>;

public:
  MySqlOperator(const std::string &host, const std::string &port,
                const std::string &user, const std::string &password,
                const std::string &schema, size_t poolSize = 5);
  MySqlOperator();
  ~MySqlOperator();

public:
  bool SaveService(size_t from, size_t to, const std::string &context);

private:
  std::unique_ptr<MySqlPool> pool;
};
