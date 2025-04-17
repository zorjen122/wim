#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
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

#include "Configer.h"
#include "Const.h"
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

  MysqlPool(const std::string &host, unsigned int port, const std::string &user,
            const std::string &passwd, const std::string &schema,
            std::size_t maxSize = 2)
      : host(host), port(port), user(user), password(passwd), schema(schema),
        stopEnable(false) {
    for (int i = 0; i < maxSize; ++i) {
      try {
        mysqlx::Session *sqlSession(
            new mysqlx::Session(host, port, user, password, schema));

        auto currentTime = std::chrono::system_clock::now().time_since_epoch();
        int64_t leaseTime =
            std::chrono::duration_cast<std::chrono::seconds>(currentTime)
                .count();
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

      return !pool.empty();
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
    std::unique_lock<std::mutex> lock(sqlMutex);

    while (!pool.empty()) {
      pool.pop();
      LOG_INFO(wim::dbLogger, "Mysql-client pool destroy! | poolSize: {}",
               pool.size() + 1);
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
  unsigned int port;
  std::string user;
  std::string password;
  std::string schema;

  std::queue<std::unique_ptr<SqlConnection>> pool;
  std::mutex sqlMutex;
  std::condition_variable condVar;
  std::atomic<bool> stopEnable;
  std::thread keepThread;
}; // MysqlPool

struct User {
  using Ptr = std::shared_ptr<User>;
  User() = default;
  User(size_t id, size_t uid, std::string username, std::string password,
       std::string email, std::string createTime = "")
      : id(std::move(id)), uid(std::move(uid)), username(std::move(username)),
        password(std::move(password)), email(std::move(email)),
        createTime(std::move(createTime)) {}
  size_t id;
  size_t uid;
  std::string username;
  std::string password;
  std::string email;
  std::string createTime;
};

struct UserInfo {
  using Ptr = std::shared_ptr<UserInfo>;

  UserInfo(size_t uid, std::string name, short age, std::string sex,
           std::string headImageURI)
      : uid(std::move(uid)), name(std::move(name)), age(std::move(age)),
        sex(std::move(sex)), headImageURI(std::move(headImageURI)) {}
  size_t uid;
  std::string name;
  short age;
  std::string sex;
  std::string headImageURI;
};

class MysqlDao : public Singleton<MysqlDao>,
                 public std::enable_shared_from_this<MysqlDao> {
private:
  MysqlPool::Ptr mysqlPool;

public:
  using Ptr = std::shared_ptr<MysqlDao>;
  MysqlDao() {
    auto conf = Configer::getConfig("server");

    auto host = conf["mysql"]["host"].as<std::string>();
    auto port = conf["mysql"]["port"].as<unsigned short>();
    auto user = conf["mysql"]["user"].as<std::string>();
    auto passwd = conf["mysql"]["password"].as<std::string>();
    auto schema = conf["mysql"]["schema"].as<std::string>();
    auto clientCount = conf["mysql"]["clientCount"].as<int>();
    mysqlPool.reset(
        new MysqlPool(host, port, user, passwd, schema, clientCount));
    if (mysqlPool->Empty()) {
      dbLogger->warn("Mysql-client pool init error! | poolSize: {}",
                     clientCount);
    } else {
      dbLogger->info("Mysql-client pool init success! | poolSize: {}",
                     clientCount);
    }
  }
  ~MysqlDao() { mysqlPool->Close(); }
  User::Ptr getUser(const std::string &username) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!, username: {}", username);
      return nullptr;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasUser = R"(SELECT * FROM users WHERE username = ?)";
      auto result = con->session->sql(hasUser).bind(username).execute();
      auto row = result.fetchOne();
      if (row.isNull()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return nullptr;
      }

      size_t id = row[0].get<size_t>();
      size_t uid = row[1].get<size_t>();
      std::string username = row[2].get<std::string>();
      std::string password = row[3].get<std::string>();
      std::string email = row[4].get<std::string>();
      std::string createDateTime = row[5].get<std::string>();
      return std::make_shared<User>(std::move(id), std::move(uid),
                                    std::move(username), std::move(password),
                                    std::move(email), createDateTime);
    } catch (mysqlx::Error &e) {
      LOG_WARN(dbLogger, "Error: {}", e.what());
      return nullptr;
    }
  }

  struct UserInfo {
    using Ptr = std::shared_ptr<UserInfo>;

    UserInfo(size_t uid, std::string name, short age, std::string sex,
             std::string headImageURL)
        : uid(std::move(uid)), name(std::move(name)), age(std::move(age)),
          sex(std::move(sex)), headImageURL(std::move(headImageURL)) {}

    size_t uid;
    std::string name;
    short age;
    std::string sex;
    std::string headImageURL;
  };
  UserInfo::Ptr getUserInfo(int uid) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!, uid: {}", uid);
      return nullptr;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f = R"(SELECT * FROM userInfo WHERE uid = ?)";
      auto result = con->session->sql(f).bind(uid).execute();
      auto row = result.fetchOne();
      if (row.isNull()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return nullptr;
      }
      std::string name = row[1].get<std::string>();
      short age = row[2].get<int>();
      std::string sex = row[3].get<std::string>();
      std::string headImageURL = row[4].get<std::string>();
      return std::make_shared<UserInfo>(std::move(uid), std::move(name),
                                        std::move(age), std::move(sex),
                                        std::move(headImageURL));

    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return nullptr;
    }
  }

  struct Friend {
    using Ptr = std::shared_ptr<Friend>;
    using FriendGroup = std::shared_ptr<std::vector<Friend::Ptr>>;
    Friend(size_t uidA, size_t uidB, std::string createDateTime,
           size_t machineId)
        : uidA(uidA), uidB(uidB), createDateTime(std::move(createDateTime)),
          machineId(machineId) {}

    static std::string formatMachineId(size_t id) {
      switch (id) {
      case 1:
        return "hunan";
      case 2:
        return "beijing";
      case 3:
        return "shanghai";
      default:
        return "";
      }
    }
    size_t uidA;
    size_t uidB;
    std::string createDateTime;
    size_t machineId;
  };

  struct FriendApply {
    using Ptr = std::shared_ptr<FriendApply>;
    using FriendApplyGroup = std::shared_ptr<std::vector<FriendApply::Ptr>>;
    FriendApply(size_t fromUid, size_t toUId, short status = 0)
        : fromUid(fromUid), toUid(toUId) {}

    std::string formatStatus(short status) {
      return status == 0 ? "wait" : "done";
    }
    size_t fromUid;
    size_t toUid;
    short status;
  };

  Friend::FriendGroup getFriendList(int uid) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!, uid: {}", uid);
      return nullptr;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f = R"(SELECT * FROM friends WHERE uidA = ?)";
      auto result = con->session->sql(f).bind(uid).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return nullptr;
      }
      auto friendGroup = std::make_shared<std::vector<Friend::Ptr>>();

      for (const auto &row : result.fetchAll()) {
        size_t uidA = row[0].get<size_t>();
        size_t uidB = row[1].get<size_t>();
        std::string createDateTime = row[2].get<std::string>();
        size_t machineId = row[3].get<size_t>();
        Friend::Ptr friendA(new Friend(uidA, uidB, createDateTime, machineId));
        friendGroup->push_back(friendA);
      }

      return friendGroup;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return nullptr;
    }
  }

  int userModifyPassword(User::Ptr user) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasUser =
          R"(SELECT * FROM users WHERE email = ? AND uid = ?)";
      auto result = con->session->sql(hasUser)
                        .bind(user->email)
                        .bind(user->uid)
                        .execute();
      auto row = result.fetchOne();
      if (row.isNull()) {
        // log...
        return 1;
      }
      std::string f =
          R"(UPDATE users SET password = ? WHERE email = ? AND uid = ?)";
      result = con->session->sql(f)
                   .bind(user->password)
                   .bind(user->email)
                   .bind(user->uid)
                   .execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }

  int userRegister(User::Ptr user) {

    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasUser =
          R"(SELECT * FROM users WHERE email = ? AND uid = ?)";
      auto result = con->session->sql(hasUser)
                        .bind(user->email)
                        .bind(user->uid)
                        .execute();
      auto row = result.fetchOne();
      if (!row.isNull()) {
        // log...
        return 1;
      }

      std::string insertUser = R"(INSERT INTO users VALUES (NULL,?,?,?,?,?))";

      auto createDateTime = getCurrentDateTime();
      result = con->session->sql(insertUser)
                   .bind(user->uid)
                   .bind(user->username)
                   .bind(user->password)
                   .bind(user->email)
                   .bind(createDateTime)
                   .execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }

  int insertUserInfo(UserInfo::Ptr userInfo) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasUserInfo = R"(SELECT * FROM userInfo WHERE uid = ?)";
      auto result =
          con->session->sql(hasUserInfo).bind(userInfo->uid).execute();
      auto row = result.fetchOne();
      if (!row.isNull()) {
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        // log...
        return 1;
      }

      std::string f = R"(INSERT INTO userInfo VALUES (?,?,?,?,?))";
      result = con->session->sql(f)
                   .bind(userInfo->uid)
                   .bind(userInfo->name)
                   .bind(userInfo->age)
                   .bind(userInfo->sex)
                   .bind(userInfo->headImageURL)
                   .execute();

      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int insertFriendApply(FriendApply::Ptr friendData) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasFriend =
          R"(SELECT * FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
      auto result = con->session->sql(hasFriend)
                        .bind(friendData->fromUid)
                        .bind(friendData->toUid)
                        .execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      static const short status = 0;
      std::string f = R"(INSERT INTO friendApplys VALUES (?, ?, ?))";
      result = con->session->sql(f)
                   .bind(friendData->fromUid)
                   .bind(friendData->toUid)
                   .bind(status)
                   .execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  // @return 0: 成功 1: 失败 -1: 组件异常
  int hasFriend(int uidA, int uidB) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });
    try {
      std::string hasFriend =
          R"(SELECT * FROM friends WHERE uidA = ? AND uidB = ?)";
      auto result =
          con->session->sql(hasFriend).bind(uidA).bind(uidB).execute();
      auto row = result.fetchOne();
      if (!row) {
        // log...
        return 1;
      }
      return 0;
    } catch (mysqlx::Error &e) {
      // log...
      LOG_DEBUG(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int insertFriend(Friend::Ptr friendData) {
    auto con = mysqlPool->GetConnection();
    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      int status = hasFriend(friendData->uidA, friendData->uidB);
      if (status == 0)
        return 1;

      std::string dateTime = getCurrentDateTime();
      std::string f = R"(INSERT INTO friends VALUES (?, ?, ?, ?))";
      auto result = con->session->sql(f)
                        .bind(friendData->uidA)
                        .bind(friendData->uidB)
                        .bind(dateTime)
                        .bind(friendData->machineId)
                        .execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  struct Message {
    using Ptr = std::shared_ptr<Message>;
    using MessageGroup = std::shared_ptr<std::vector<Message::Ptr>>;
    Message(int messageId, int fromUid, int toUid, std::string sessionKey,
            short type, std::string content, short status,
            std::string sendDateTime, std::string readDateTime = "")
        : messageId(messageId), fromUid(fromUid), toUid(toUid), type(type),
          content(std::move(content)), status(status),
          sendDateTime(std::move(sendDateTime)),
          readDateTime(std::move(readDateTime)) {}

    std::string formatType(short type) {
      switch (type) {
      case 1:
        return "text";
      case 2:
        return "image";
      case 3:
        return "audio";
      case 4:
        return "video";
      case 5:
        return "file";
      default:
        LOG_DEBUG(dbLogger, "no such type value: {}", type);
        return "";
      }
    }
    std::string formatStatus(short status) {
      switch (status) {
      case 0:
        return "withdraw"; // 撤回
      case 1:
        return "wait";
      case 2:
        return "done";
      default:
        LOG_DEBUG(dbLogger, "no such status value: {}", status);
        return "";
      }
    }
    int messageId;
    int fromUid;
    int toUid;
    std::string sessionKey;
    short type;
    std::string content;
    short status;
    std::string sendDateTime;
    std::string readDateTime;
  };
  int insertMessage(Message::Ptr message) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f =
          R"(INSERT INTO messages VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?))";
      auto result = con->session->sql(f)
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
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int updateUserInfoName(int uid, const std::string &name) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f = R"(UPDATE userInfo SET name = ? WHERE uid = ?)";
      auto result = con->session->sql(f).bind(name).bind(uid).execute();

      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int updateUserInfoAge(int uid, int age) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f = R"(UPDATE userInfo SET age = ? WHERE uid = ?)";
      auto result = con->session->sql(f).bind(uid).bind(age).execute();

      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int updateUserInfoSex(int uid, const std::string &sex) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f = R"(UPDATE userInfo SET sex = ? WHERE uid = ?)";
      auto result = con->session->sql(f).bind(sex).bind(uid).execute();

      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int updateUserInfoHeadImageURL(int uid, const std::string &headImageURL) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f = R"(UPDATE userInfo SET headImageURL = ? WHERE uid = ?)";
      auto result = con->session->sql(f).bind(headImageURL).bind(uid).execute();

      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int updateMessage(int messageId, short status) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasMessage = R"(SELECT * FROM messages WHERE messageId = ?)";
      auto result = con->session->sql(hasMessage).bind(messageId).execute();
      auto row = result.fetchOne();
      if (row.isNull()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      std::string f = R"(UPDATE messages SET status = ? WHERE messageId = ?)";
      result = con->session->sql(f).bind(status).bind(messageId).execute();

      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  FriendApply::FriendApplyGroup getFriendApplyList(int from) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return nullptr;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      const static short STATUS = 0;
      std::string f =
          R"(SELECT * FROM friendApplys WHERE status = ? AND fromUid = ?)";
      auto result = con->session->sql(f).bind(STATUS).bind(from).execute();
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
        short status = row[2].get<int>();
        friendApply.reset(new FriendApply(fromUid, toUid, status));
        friendApplyGroup->push_back(friendApply);
      }
      return friendApplyGroup;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return nullptr;
    }
  }

  Message::MessageGroup getUserMessage(int uid, int startMessageId,
                                       int pullCount) {
    if (startMessageId <= 0)
      return nullptr;
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return nullptr;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string f =
          R"(SELECT * FROM messages WHERE receiverId = ? AND messageId >= ? ORDER BY messageId DESC LIMIT ?)";
      auto result = con->session->sql(f)
                        .bind(uid)
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
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return nullptr;
    }
  }
  int deleteUser(int uid) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasUser =
          R"(SELECT a.uid FROM users AS a, userInfo AS b WHERE a.uid = ? AND a.uid = b.uid)";
      auto result = con->session->sql(hasUser).bind(uid).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      std::string f1 = R"(DELETE FROM users WHERE uid = ?)";
      std::string f2 = R"(DELETE FROM userInfo WHERE uid = ?)";
      result = con->session->sql(f1).bind(uid).execute();
      result = con->session->sql(f2).bind(uid).execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int deleteFriendApply(int fromUid, int toUid) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasFriendApply =
          R"(SELECT fromUid FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
      auto result =
          con->session->sql(hasFriendApply).bind(fromUid).bind(toUid).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      std::string f =
          R"(DELETE FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
      result = con->session->sql(f).bind(fromUid).bind(toUid).execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
  int deleteFriend(int uidA, int uidB) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return 1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      std::string hasFriend =
          R"(SELECT uidA FROM friends WHERE uidA = ? AND uidB = ?)";
      auto result =
          con->session->sql(hasFriend).bind(uidA).bind(uidB).execute();
      if (!result.hasData()) {
        // log...
        LOG_DEBUG(dbLogger, "result fetchOne is null");
        return 1;
      }
      std::string f = R"(DELETE FROM friends WHERE uidA = ? AND uidB = ?)";
      result = con->session->sql(f).bind(uidA).bind(uidB).execute();
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }

  // todo...
  int deleteMessage(int uid, int startMessageId, int delCount) {
    auto con = mysqlPool->GetConnection();

    if (con == nullptr) {
      // log...
      LOG_DEBUG(dbLogger, "pool number is empty!");
      return -1;
    }

    Defer defer(
        [&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

    try {
      return 0;
    } catch (mysqlx::Error &e) {
      LOG_INFO(dbLogger, "Error: {}", e.what());
      return -1;
    }
  }
};
}; // namespace wim::db