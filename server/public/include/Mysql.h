#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <exception>
#include <mysql-cppconn-8/mysqlx/devapi/common.h>
#include <mysql-cppconn-8/mysqlx/devapi/result.h>
#include <mysql-cppconn-8/mysqlx/xdevapi.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <type_traits>

#include "Configer.h"
#include "Const.h"
#include "DbGlobal.h"
#include "Logger.h"

namespace wim::db {

struct SqlConnection {
  SqlConnection(mysqlx::Session *connection, int64_t lastTime)
      : session(connection), leaseTime(lastTime) {}
  ~SqlConnection() {
    if (session) {
      session->close();
      session.reset();
    }
  }

  std::unique_ptr<mysqlx::Session> session;
  int64_t leaseTime;
};

class MysqlPool {
public:
  using Ptr = std::shared_ptr<MysqlPool>;

  MysqlPool(const std::string &host, unsigned short port,
            const std::string &user, const std::string &password,
            const std::string &schema, std::size_t maxSize = 2)
      : host(host), port(port), user(user), password(password), schema(schema) {
    for (size_t i = 0; i < maxSize; ++i) {
      try {
        mysqlx::Session *sqlSession(
            new mysqlx::Session(host, port, user, password, schema));

        auto currentTime = std::chrono::system_clock::now().time_since_epoch();
        int64_t leaseTime =
            std::chrono::duration_cast<std::chrono::seconds>(currentTime)
                .count();
        sqlSession->sql("SELECT 1").execute();

        pool.push(std::make_unique<SqlConnection>(sqlSession, leaseTime));

      } catch (mysqlx::Error &e) {
        LOG_WARN(wim::dbLogger, "Mysql-client init(number-{}) error: {}", i,
                 e.what());
        continue;
      }
    }

    if (pool.empty()) {
      LOG_DEBUG(wim::dbLogger, "Mysql-client pool init error! | poolSize: {}",
                maxSize);
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

public:
  std::unique_ptr<SqlConnection> GetConnection() {
    std::unique_lock<std::mutex> lock(sqlMutex);
    condVar.wait(lock, [this] {
      if (stopEnable)
        return true;

      if (pool.empty()) {
        LOG_DEBUG(wim::dbLogger, "Mysql-client pool is empty!, wait...");
        return false;
      } else {
        return true;
      }
    });
    if (stopEnable || pool.empty()) {
      return nullptr;
    }
    std::unique_ptr<SqlConnection> con(std::move(pool.front()));
    pool.pop();
    return con;
  }

  std::unique_ptr<SqlConnection>
  ReturnConnection(std::unique_ptr<SqlConnection> con) {
    std::unique_lock<std::mutex> lock(sqlMutex);
    if (stopEnable)
      return con;

    pool.push(std::move(con));
    condVar.notify_one();
    return nullptr;
  }

  bool Empty() {
    std::lock_guard<std::mutex> lock(sqlMutex);
    return pool.empty();
  }

  void Close() {
    // 每个等待锁的调用线程都会被唤醒，
    // 并且每个锁相关函数都在锁跳出的的下一步检查stopEnable标志，如果标志为true则跳出
    stopEnable = true;
    condVar.notify_all();
  }
  std::size_t Size() {
    std::lock_guard<std::mutex> lock(sqlMutex);
    return pool.size();
  }

  ~MysqlPool() {
    Close();
    std::unique_lock<std::mutex> lock(sqlMutex);
    while (!pool.empty()) {
      pool.pop();
      LOG_INFO(wim::dbLogger, "Mysql-client pool destroy! | poolSize: {}",
               pool.size());
    }
    if (keepThread.joinable()) {
      keepThread.join();
    }
  }

private:
  void keepConnectionHandle() {
    std::lock_guard<std::mutex> lock(sqlMutex);
    int poolsize = pool.size();
    // 获取当前时间戳
    auto currentTime = std::chrono::system_clock::now().time_since_epoch();
    // 将时间戳转换为秒
    long long leaseTime =
        std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

    for (int i = 0; i < poolsize; i++) {

      // 一旦Close后，就可跳出，无需继续处理
      if (stopEnable)
        return;

      auto con = std::move(pool.front());
      pool.pop();

      if (leaseTime - con->leaseTime < 5) {
        pool.push(std::move(con));
        continue;
      }

      try {
        mysqlx::SqlResult result = con->session->sql("SELECT 1").execute();
        con->leaseTime = leaseTime;
        pool.push(std::move(con));
      } catch (mysqlx::Error &e) {
        LOG_INFO(wim::dbLogger,
                 "Error keeping connection alive: {}, reconnect now...",
                 e.what());
        try {
          mysqlx::Session *newSession(
              new mysqlx::Session(host, user, password, schema));
          con->leaseTime = leaseTime;
          con.reset();
          pool.push(std::make_unique<SqlConnection>(newSession, leaseTime));
        } catch (mysqlx::Error &e) {
          LOG_INFO(wim::dbLogger, "Error reconnecting: {}", e.what());
          return;
        }
      }
    }
  }

private:
  std::string host;
  unsigned short port;
  std::string user;
  std::string password;
  std::string schema;

  std::queue<std::unique_ptr<SqlConnection>> pool{};
  std::mutex sqlMutex{};
  std::condition_variable condVar{};
  std::atomic<bool> stopEnable{};
  std::thread keepThread{};
}; // MysqlPool

class MysqlDao : public Singleton<MysqlDao>,
                 public std::enable_shared_from_this<MysqlDao> {
private:
  MysqlPool::Ptr mysqlPool;
  friend class TestDb;

public:
  using Ptr = std::shared_ptr<MysqlDao>;
  MysqlDao() {
    try {
      auto conf = Configer::getNode("server");

      auto host = conf["mysql"]["host"].as<std::string>();
      auto port = conf["mysql"]["port"].as<unsigned short>();
      auto user = conf["mysql"]["user"].as<std::string>();
      auto password = conf["mysql"]["password"].as<std::string>();
      auto schema = conf["mysql"]["schema"].as<std::string>();
      auto clientCount = conf["mysql"]["clientCount"].as<int>();
      mysqlPool.reset(
          new MysqlPool(host, port, user, password, schema, clientCount));

      if (!mysqlPool->Empty()) {
        LOG_INFO(wim::dbLogger,
                 "Mysql-Client pool init success! | host: {}, port: {}, "
                 "user: {}, passwd: {}, schema: {}, poolSize: {}",
                 host, port, user, password, schema, mysqlPool->Size());
      } else {
        LOG_WARN(wim::dbLogger,
                 "Mysql-Client pool init failed! | host: {}, port: {}, "
                 "user: {}, passwd: {}, schema: {}, poolSize: {}",
                 host, port, user, password, schema, mysqlPool->Size());
      }
    } catch (std::exception &e) {
      LOG_ERROR(wim::dbLogger, "MysqlDao init error: {}", e.what());
    }
  }
  ~MysqlDao() {}

  /*
  接口说明：【2025-5-2】
    对于int、long返回类型，返回-1表示存储时出错；对于查询，返回1表示已存在该数据
    对于指针（shared_ptr）返回类型，错误时统一返回nullptr
    对于bool返回类型，false表示错误
    返回接口定义处见：defaultForType
  注意：为避免死锁，接口函数之间不得互相调用，因每个调用都会获取连接池，若池中没有可用连接，则会一直等待
  */
  template <typename Func>
  auto executeTemplate(Func &&processor) -> decltype(
      processor(std::declval<std::unique_ptr<mysqlx::Session> &>())) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      LOG_INFO(dbLogger, "Mysql connection pool is empty!");
      return defaultForType<decltype(processor(con->session))>();
    }
    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      return processor(con->session); // 完全由processor处理SQL执行
    } catch (mysqlx::Error &e) {
      LOG_ERROR(dbLogger, "MySQL error: {}", e.what());
      HandleError(e);
      return defaultForType<decltype(processor(con->session))>();
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
  void HandleError(mysqlx::Error &e) {
    // 暂无处理
    return;
  }

  void Close() { mysqlPool->Close(); }

  User::Ptr getUser(const std::string &username) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> User::Ptr {
          auto result = session->sql("SELECT * FROM users WHERE username = ?")
                            .bind(username)
                            .execute();
          auto row = result.fetchOne();
          if (row.isNull()) {
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return nullptr;
          }

          size_t id = row[0].get<size_t>();
          size_t uid = row[1].get<size_t>();
          std::string username = row[2].get<std::string>();
          std::string password = row[3].get<std::string>();
          std::string email = row[4].get<std::string>();
          std::string createTime = row[5].get<std::string>();
          return std::make_shared<User>(
              std::move(id), std::move(uid), std::move(username),
              std::move(password), std::move(email), createTime);
        });
  }

  UserInfo::Ptr getUserInfo(long uid) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> UserInfo::Ptr {
          auto result = session->sql("SELECT * FROM userInfo WHERE uid = ?")
                            .bind(uid)
                            .execute();
          auto row = result.fetchOne();
          if (row.isNull()) {
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return nullptr;
          }
          return std::make_shared<UserInfo>(
              uid,
              row[1].get<std::string>(), // name
              row[2].get<int>(),         // age
              row[3].get<std::string>(), // sex
              row[4].get<std::string>()  // headImageURL
          );
        });
  }

  bool hasUserInfo(long uid) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> bool {
          auto result = session->sql("SELECT uid FROM userInfo WHERE uid = ?")
                            .bind(uid)
                            .execute();
          return result.hasData();
        });
  }

  Friend::FriendGroup getFriendList(long uid) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> Friend::FriendGroup {
          auto result = session->sql("SELECT * FROM friends WHERE uidA = ?")
                            .bind(uid)
                            .execute();

          if (!result.hasData()) {
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return nullptr;
          }

          auto friendGroup = std::make_shared<std::vector<Friend::Ptr>>();
          for (const auto &row : result.fetchAll()) {
            friendGroup->emplace_back(
                new Friend(row[0].get<size_t>(),      // uidA
                           row[1].get<size_t>(),      // uidB
                           row[3].get<std::string>(), // createTime
                           row[2].get<size_t>()       // sessionId
                           ));
          }
          return friendGroup;
        });
  }

  int userModifyPassword(User::Ptr user) {
    return executeTemplate([user](std::unique_ptr<mysqlx::Session> &session)
                               -> int {
      // 验证用户存在
      auto checkResult =
          session->sql("SELECT * FROM users WHERE email = ? AND uid = ?")
              .bind(user->email)
              .bind(user->uid)
              .execute();

      if (checkResult.fetchOne().isNull()) {
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1; // 用户不存在
      }

      // 更新密码
      auto updateResult =
          session
              ->sql("UPDATE users SET password = ? WHERE email = ? AND uid = ?")
              .bind(user->password)
              .bind(user->email)
              .bind(user->uid)
              .execute();

      return 0; // 成功
    });
  }

  int userRegister(User::Ptr user) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          // 检查用户是否已存在
          auto checkResult =
              session->sql("SELECT * FROM users WHERE email = ? OR uid = ?")
                  .bind(user->email)
                  .bind(user->uid)
                  .execute();

          if (!checkResult.fetchOne().isNull()) {
            return 1; // 用户已存在
          }

          // 注册新用户
          auto createTime = getCurrentDateTime();
          auto insertResult =
              session->sql("INSERT INTO users VALUES (NULL,?,?,?,?,?)")
                  .bind(user->uid)
                  .bind(user->username)
                  .bind(user->password)
                  .bind(user->email)
                  .bind(createTime)
                  .execute();

          return 0; // 注册成功
        });
  }

  int insertUserInfo(UserInfo::Ptr userInfo) {

    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string hasUserInfo = R"(SELECT * FROM userInfo WHERE uid = ?)";
          auto result = session->sql(hasUserInfo).bind(userInfo->uid).execute();
          auto row = result.fetchOne();
          if (!row.isNull()) {
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            // log...
            return 1;
          }

          std::string f = R"(INSERT INTO userInfo VALUES (?,?,?,?,?))";
          result = session->sql(f)
                       .bind(userInfo->uid)
                       .bind(userInfo->name)
                       .bind(userInfo->age)
                       .bind(userInfo->sex)
                       .bind(userInfo->headImageURL)
                       .execute();

          return 0;
        });
  }

  int insertFriendApply(FriendApply::Ptr friendData) {

    return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                               -> int {
      std::string hasFriend =
          R"(SELECT COUNT(*) FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
      auto count = session->sql(hasFriend)
                       .bind(friendData->fromUid)
                       .bind(friendData->toUid)
                       .execute()
                       .fetchOne()[0]
                       .get<int>();
      if (count > 0) {
        LOG_DEBUG(dbLogger, "Record already exists");
        return 1;
      }
      std::string f = R"(INSERT INTO friendApplys VALUES (?, ?, ?, ?, ?))";

      auto result = session->sql(f)
                        .bind(friendData->fromUid)
                        .bind(friendData->toUid)
                        .bind(friendData->content)
                        .bind(friendData->status)
                        .bind(friendData->createTime)
                        .execute();
      return 0;
    });
  }

  int updateFriendApplyStatus(FriendApply::Ptr friendData) {
    return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                               -> int {
      // update: status, content, createTime
      std::string f =
          R"(UPDATE friendApplys SET status = ?, content = ?, createTime = ? WHERE fromUid = ? AND toUid = ?)";
      auto result = session->sql(f)
                        .bind(friendData->status)
                        .bind(friendData->content)
                        .bind(friendData->createTime)
                        .bind(friendData->fromUid)
                        .bind(friendData->toUid)
                        .execute();

      return 0;
    });
  }
  int hasFriend(long uidA, long uidB) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string hasFriend =
              R"(SELECT * FROM friends WHERE uidA = ? AND uidB = ?)";
          auto result = session->sql(hasFriend).bind(uidA).bind(uidB).execute();
          auto row = result.fetchOne();
          if (!row) {
            // log...
            return 1;
          }
          return 0;
        });
  }
  int insertFriend(Friend::Ptr friendData) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string hasFriend =
              R"(SELECT COUNT(*) FROM friends WHERE uidA = ? AND uidB = ?)";
          auto result = session->sql(hasFriend)
                            .bind(friendData->uidA)
                            .bind(friendData->uidB)
                            .execute();
          auto row = result.fetchOne();
          if (!row.isNull()) {
            return 1;
          }
          std::string dateTime = getCurrentDateTime();
          std::string f = R"(INSERT INTO friends VALUES (?, ?, ?, ?))";
          result = session->sql(f)
                       .bind(friendData->uidA)
                       .bind(friendData->uidB)
                       .bind(friendData->sessionId)
                       .bind(dateTime)
                       .execute();
          return 0;
        });
  }

  int insertMessage(Message::Ptr message) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f =
              R"(INSERT INTO messages VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?))";
          auto result = session->sql(f)
                            .bind(message->messageId)
                            .bind(message->fromUid)
                            .bind(message->toUid)
                            .bind(message->sessionKey)
                            .bind(message->type)
                            .bind(message->content)
                            .bind(message->status)
                            .bind(message->sendDateTime)
                            .bind(message->readDateTime)
                            .execute();

          return 0;
        });
  }
  int updateUserInfoName(long uid, const std::string &name) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f = R"(UPDATE userInfo SET name = ? WHERE uid = ?)";
          auto result = session->sql(f).bind(name).bind(uid).execute();

          return 0;
        });
  }
  int updateUserInfoAge(long uid, int age) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f = R"(UPDATE userInfo SET age = ? WHERE uid = ?)";
          auto result = session->sql(f).bind(uid).bind(age).execute();

          return 0;
        });
  }
  int updateUserInfoSex(long uid, const std::string &sex) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f = R"(UPDATE userInfo SET sex = ? WHERE uid = ?)";
          auto result = session->sql(f).bind(sex).bind(uid).execute();

          return 0;
        });
  }
  int updateUserInfoHeadImageURL(long uid, const std::string &headImageURL) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f =
              R"(UPDATE userInfo SET headImageURL = ? WHERE uid = ?)";
          auto result = session->sql(f).bind(headImageURL).bind(uid).execute();

          return 0;
        });
  }
  int updateMessage(long messageId, short status) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string hasMessage =
              R"(SELECT * FROM messages WHERE messageId = ?)";
          auto result = session->sql(hasMessage).bind(messageId).execute();
          auto row = result.fetchOne();
          if (row.isNull()) {
            // log...
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return 1;
          }
          std::string f =
              R"(UPDATE messages SET status = ? WHERE messageId = ?)";
          result = session->sql(f).bind(status).bind(messageId).execute();

          return 0;
        });
  }
  FriendApply::FriendApplyGroup getFriendApplyList(long from) {
    return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                               -> FriendApply::FriendApplyGroup {
      const static short STATUS = 0;
      std::string f =
          R"(SELECT * FROM friendApplys WHERE status = ? AND fromUid = ?)";
      auto result = session->sql(f).bind(STATUS).bind(from).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return nullptr;
      }
      FriendApply::FriendApplyGroup friendApplyGroup(
          new std::vector<FriendApply::Ptr>());
      FriendApply::Ptr friendApply;
      for (auto row : result.fetchAll()) {
        size_t fromUid = row[0].get<size_t>();
        size_t toUid = row[1].get<size_t>();
        std::string content = row[2].get<std::string>();
        short status = row[3].get<int>();
        std::string createTime = row[4].get<std::string>();
        friendApply.reset(
            new FriendApply(fromUid, toUid, status, content, createTime));
        friendApplyGroup->push_back(friendApply);
      }
      return friendApplyGroup;
    });
  }

  Message::MessageGroup getUserMessage(long from, long to, int startMessageId,
                                       int pullCount) {
    if (startMessageId <= 0)
      return nullptr;
    return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                               -> Message::MessageGroup {
      std::string f =
          R"(SELECT * FROM messages WHERE senderId = ? AND receiverId = ? AND messageId >= ? ORDER BY messageId DESC LIMIT ?)";
      auto result = session->sql(f)
                        .bind(from)
                        .bind(to)
                        .bind(startMessageId)
                        .bind(pullCount)
                        .execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return nullptr;
      }
      Message::MessageGroup messageGroup(new std::vector<Message::Ptr>());
      Message::Ptr messagePtr;
      for (auto row : result.fetchAll()) {
        size_t messageId = row[0].get<size_t>();
        size_t fromUid = row[1].get<size_t>();
        size_t toUid = row[2].get<size_t>();
        std::string sessionKey = row[3].get<std::string>();
        short type = row[4].get<int>();
        std::string content = row[5].get<std::string>();
        short status = row[6].get<int>();
        std::string sendDateTime = row[7].get<std::string>();
        std::string readDateTime = row[8].get<std::string>();

        messagePtr.reset(
            new Message(messageId, fromUid, toUid, std::move(sessionKey), type,
                        std::move(content), status, std::move(sendDateTime),
                        std::move(readDateTime)));
        messageGroup->push_back(messagePtr);
      }
      return messageGroup;
    });
  }
  int deleteUser(long uid) {
    return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                               -> int {
      std::string hasUser =
          R"(SELECT a.uid FROM users AS a, userInfo AS b WHERE a.uid = ? AND a.uid = b.uid)";
      auto result = session->sql(hasUser).bind(uid).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      std::string f1 = R"(DELETE FROM users WHERE uid = ?)";
      std::string f2 = R"(DELETE FROM userInfo WHERE uid = ?)";
      result = session->sql(f1).bind(uid).execute();
      result = session->sql(f2).bind(uid).execute();
      return 0;
    });
  }
  int deleteFriendApply(long fromUid, long toUid) {
    return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                               -> int {
      std::string hasFriendApply =
          R"(SELECT fromUid FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
      auto result =
          session->sql(hasFriendApply).bind(fromUid).bind(toUid).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      std::string f =
          R"(DELETE FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
      result = session->sql(f).bind(fromUid).bind(toUid).execute();
      return 0;
    });
  }
  int deleteFriend(long uidA, long uidB) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string hasFriend =
              R"(SELECT uidA FROM friends WHERE uidA = ? AND uidB = ?)";
          auto result = session->sql(hasFriend).bind(uidA).bind(uidB).execute();
          if (!result.hasData()) {
            // log...
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return 1;
          }
          std::string f = R"(DELETE FROM friends WHERE uidA = ? AND uidB = ?)";
          result = session->sql(f).bind(uidA).bind(uidB).execute();
          return 0;
        });
  }

  // todo...
  int deleteMessage(long uid, int startMessageId, int delCount) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int { return 0; });
  }

  int hasEmail(std::string email) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f = R"(SELECT email FROM users WHERE email = ?)";
          auto result = session->sql(f).bind(email).execute();
          if (!result.hasData()) {
            // log...
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return 1;
          }
          return 0;
        });
  }
  int hasUsername(std::string username) {
    return executeTemplate(
        [&](std::unique_ptr<mysqlx::Session> &session) -> int {
          std::string f = R"(SELECT username FROM users WHERE username = ?)";
          auto result = session->sql(f).bind(username).execute();
          if (!result.hasData()) {
            // log...
            LOG_DEBUG(dbLogger, "result fetchOne is null");
            return 1;
          }
          return 0;
        });
  }
};
}; // namespace wim::db
