#include "Mysql.h"
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

SqlConnection::SqlConnection(mysqlx::Session *connection, int64_t lastTime)
    : session(connection), leaseTime(lastTime) {}
SqlConnection::~SqlConnection() {
  if (session) {
    session->close();
    session.reset();
  }
}

MysqlPool::MysqlPool(const std::string &host, unsigned short port,
                     const std::string &user, const std::string &password,
                     const std::string &schema, std::size_t maxSize)
    : host(host), port(port), user(user), password(password), schema(schema) {
  for (size_t i = 0; i < maxSize; ++i) {
    try {
      mysqlx::Session *sqlSession(
          new mysqlx::Session(host, port, user, password, schema));

      auto currentTime = std::chrono::system_clock::now().time_since_epoch();
      int64_t leaseTime =
          std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
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

std::unique_ptr<SqlConnection> MysqlPool::GetConnection() {
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
MysqlPool::ReturnConnection(std::unique_ptr<SqlConnection> con) {
  std::unique_lock<std::mutex> lock(sqlMutex);
  if (stopEnable)
    return con;

  pool.push(std::move(con));
  condVar.notify_one();
  return nullptr;
}

bool MysqlPool::Empty() {
  std::lock_guard<std::mutex> lock(sqlMutex);
  return pool.empty();
}

void MysqlPool::Close() {
  // 每个等待锁的调用线程都会被唤醒，
  // 并且每个锁相关函数都在锁跳出的的下一步检查stopEnable标志，如果标志为true则跳出
  stopEnable = true;
  condVar.notify_all();
}
std::size_t MysqlPool::Size() {
  std::lock_guard<std::mutex> lock(sqlMutex);
  return pool.size();
}

MysqlPool::~MysqlPool() {
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

void MysqlPool::keepConnectionHandle() {
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

MysqlDao::MysqlDao() {
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
MysqlDao::~MysqlDao() {}

/*
接口说明：【2025-5-2】
  对于int、long返回类型，返回-1表示存储时出错；对于查询，返回1表示已存在该数据
  对于指针（shared_ptr）返回类型，错误时统一返回nullptr
  对于bool返回类型，false表示错误
  返回接口定义处见：defaultForType
注意：为避免死锁，接口函数之间不得互相调用，因每个调用都会获取连接池，若池中没有可用连接，则会一直等待
*/
template <typename Func>
auto MysqlDao::executeTemplate(Func &&processor)
    -> decltype(processor(std::declval<std::unique_ptr<mysqlx::Session> &>())) {
  auto con = mysqlPool->GetConnection();
  if (con == nullptr) {
    LOG_INFO(dbLogger, "Mysql connection pool is empty!");
    return defaultForType<decltype(processor(con->session))>();
  }
  Defer defer([&con, this]() { mysqlPool->ReturnConnection(std::move(con)); });

  try {
    return processor(con->session); // 完全由processor处理SQL执行
  } catch (mysqlx::Error &e) {
    LOG_ERROR(dbLogger, "MySQL error: {}", e.what());
    HandleError(e);
    return defaultForType<decltype(processor(con->session))>();
  }
}
template <typename T> auto MysqlDao::defaultForType() {
  if constexpr (std::is_same_v<T, bool>) {
    return false;
  } else if constexpr (std::is_integral_v<T>) {
    return -1;
  } else {
    return T{};
  }
}
void MysqlDao::HandleError(mysqlx::Error &e) {
  // 暂无处理
  return;
}

void MysqlDao::Close() { mysqlPool->Close(); }

User::Ptr MysqlDao::getUser(const std::string &username) {
  return executeTemplate(
      [&](std::unique_ptr<mysqlx::Session> &session) -> User::Ptr {
        auto result = session->sql("SELECT * FROM users WHERE username = ?")
                          .bind(username)
                          .execute();
        if (!(result.count() > 0)) {
          LOG_DEBUG(dbLogger, "Mysql指令调用的结果集为空");
          return nullptr;
        }

        auto row = result.fetchOne();

        size_t id = row[0].get<size_t>();
        size_t uid = row[1].get<size_t>();
        std::string username = row[2].get<std::string>();
        std::string password = row[3].get<std::string>();
        std::string email = row[4].get<std::string>();
        std::string createTime = row[5].get<std::string>();
        return std::make_shared<User>(std::move(id), std::move(uid),
                                      std::move(username), std::move(password),
                                      std::move(email), createTime);
      });
}

UserInfo::Ptr MysqlDao::getUserInfo(long uid) {
  return executeTemplate(
      [&](std::unique_ptr<mysqlx::Session> &session) -> UserInfo::Ptr {
        auto result = session->sql("SELECT * FROM userInfo WHERE uid = ?")
                          .bind(uid)
                          .execute();
        if (!(result.count() > 0)) {
          LOG_DEBUG(dbLogger, "Mysql指令调用的结果集为空");
          return nullptr;
        }
        auto row = result.fetchOne();
        return std::make_shared<UserInfo>(
            uid,
            row[1].get<std::string>(), // name
            row[2].get<int>(),         // age
            row[3].get<std::string>(), // sex
            row[4].get<std::string>()  // headImageURL
        );
      });
}

bool MysqlDao::hasUserInfo(long uid) {
  return executeTemplate(
      [&](std::unique_ptr<mysqlx::Session> &session) -> bool {
        auto result = session->sql("SELECT 1 FROM userInfo WHERE uid = ?")
                          .bind(uid)
                          .execute();
        return (result.fetchOne() != 0);
      });
}

Friend::FriendGroup MysqlDao::getFriendList(long uid) {
  return executeTemplate(
      [&](std::unique_ptr<mysqlx::Session> &session) -> Friend::FriendGroup {
        auto result = session->sql("SELECT * FROM `friends` WHERE uidA = ?")
                          .bind(uid)
                          .execute();

        if (!(result.count() > 0)) {
          LOG_DEBUG(dbLogger, "Mysql指令调用的结果集为空");
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

int MysqlDao::userModifyPassword(User::Ptr user) {
  return executeTemplate([user](
                             std::unique_ptr<mysqlx::Session> &session) -> int {
    // 验证用户存在
    auto checkResult =
        session->sql("SELECT * FROM users WHERE email = ? AND uid = ?")
            .bind(user->email)
            .bind(user->uid)
            .execute();

    if (!(checkResult.count() > 0)) {
      LOG_DEBUG(dbLogger, "Mysql指令调用的结果集为空");
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

int MysqlDao::userRegister(User::Ptr user) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    // 检查用户是否已存在
    auto result =
        session->sql("SELECT COUNT(*) FROM users WHERE email = ? OR uid = ?")
            .bind(user->email)
            .bind(user->uid)
            .execute();

    if ((result.count() > 0)) {
      LOG_INFO(dbLogger, "User already exists");
      return 1;
    }

    // 注册新用户
    auto createTime = getCurrentDateTime();
    result = session->sql("INSERT INTO users VALUES (NULL,?,?,?,?,?)")
                 .bind(user->uid)
                 .bind(user->username)
                 .bind(user->password)
                 .bind(user->email)
                 .bind(createTime)
                 .execute();

    return 0; // 注册成功
  });
}

int MysqlDao::insertUserInfo(UserInfo::Ptr userInfo) {

  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(INSERT INTO userInfo VALUES (?,?,?,?,?))";
    auto result = session->sql(f)
                      .bind(userInfo->uid)
                      .bind(userInfo->name)
                      .bind(userInfo->age)
                      .bind(userInfo->sex)
                      .bind(userInfo->headImageURL)
                      .execute();

    return 0;
  });
}

int MysqlDao::insertFriendApply(FriendApply::Ptr friendData) {

  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(INSERT INTO friendApplys VALUES (?, ?, ?, ?, ?))";
    // 存两份，便于两边查找
    auto result = session->sql(f)
                      .bind(friendData->from)
                      .bind(friendData->to)
                      .bind(friendData->content)
                      .bind(friendData->status)
                      .bind(friendData->createTime)
                      .execute();
    result = session->sql(f)
                 .bind(friendData->to)
                 .bind(friendData->from)
                 .bind(friendData->content)
                 .bind(friendData->status)
                 .bind(friendData->createTime)
                 .execute();
    return 0;
  });
}

int MysqlDao::updateFriendApplyStatus(FriendApply::Ptr friendData) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    // update: status, content, createTime
    std::string f =
        R"(UPDATE friendApplys SET status = ?, content = ?, createTime = ? WHERE fromUid = ? AND toUid = ?)";
    auto result = session->sql(f)
                      .bind(friendData->status)
                      .bind(friendData->content)
                      .bind(friendData->createTime)
                      .bind(friendData->from)
                      .bind(friendData->to)
                      .execute();

    result = session->sql(f)
                 .bind(friendData->status)
                 .bind(friendData->content)
                 .bind(friendData->createTime)
                 .bind(friendData->to)
                 .bind(friendData->from)
                 .execute();
    return 0;
  });
}
int MysqlDao::hasFriend(long uidA, long uidB) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string hasFriend =
        R"(SELECT 1FROM `friends` WHERE uidA = ? AND uidB = ?)";
    auto result = session->sql(hasFriend).bind(uidA).bind(uidB).execute();
    return (result.fetchOne() != 0);
  });
}
int MysqlDao::insertFriend(Friend::Ptr friendData) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string dateTime = getCurrentDateTime();
    std::string f = R"(INSERT INTO `friends` VALUES (?, ?, ?, ?))";
    auto result = session->sql(f)
                      .bind(friendData->uidA)
                      .bind(friendData->uidB)
                      .bind(friendData->sessionId)
                      .bind(dateTime)
                      .execute();
    result = session->sql(f)
                 .bind(friendData->uidB)
                 .bind(friendData->uidA)
                 .bind(friendData->sessionId)
                 .bind(dateTime)
                 .execute();
    return 0;
  });
}

int MysqlDao::insertMessage(Message::Ptr message) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f =
        R"(INSERT INTO messages VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?))";
    auto result = session->sql(f)
                      .bind(message->messageId)
                      .bind(message->from)
                      .bind(message->to)
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
int MysqlDao::updateUserInfoName(long uid, const std::string &name) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(UPDATE userInfo SET name = ? WHERE uid = ?)";
    auto result = session->sql(f).bind(name).bind(uid).execute();

    return 0;
  });
}
int MysqlDao::updateUserInfoAge(long uid, int age) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(UPDATE userInfo SET age = ? WHERE uid = ?)";
    auto result = session->sql(f).bind(uid).bind(age).execute();

    return 0;
  });
}
int MysqlDao::updateUserInfoSex(long uid, const std::string &sex) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(UPDATE userInfo SET sex = ? WHERE uid = ?)";
    auto result = session->sql(f).bind(sex).bind(uid).execute();

    return 0;
  });
}
int MysqlDao::updateUserInfoHeadImageURL(long uid,
                                         const std::string &headImageURL) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(UPDATE userInfo SET headImageURL = ? WHERE uid = ?)";
    auto result = session->sql(f).bind(headImageURL).bind(uid).execute();

    return 0;
  });
}
int MysqlDao::updateMessage(long messageId, short status) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(UPDATE messages SET status = ? WHERE messageId = ?)";
    auto result = session->sql(f).bind(status).bind(messageId).execute();

    return 0;
  });
}
FriendApply::FriendApplyGroup MysqlDao::getFriendApplyList(long from) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> FriendApply::FriendApplyGroup {
    const static short STATUS = 0;
    std::string f =
        R"(SELECT * FROM friendApplys WHERE status = ? AND fromUid = ?)";
    auto result = session->sql(f).bind(STATUS).bind(from).execute();
    if (!(result.count() > 0)) {
      // log...
      LOG_DEBUG(dbLogger, "Mysql指令调用的结果集为空");
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

Message::MessageGroup MysqlDao::getSessionMessage(long from, long to,
                                                  int lastMessageId,
                                                  int pullCount) {
  if (lastMessageId < 0)
    return nullptr;
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> Message::MessageGroup {
    std::string f =
        R"(SELECT * FROM messages WHERE senderId = ? AND receiverId = ? AND messageId >= ? ORDER BY messageId DESC LIMIT ?)";
    auto result = session->sql(f)
                      .bind(from)
                      .bind(to)
                      .bind(lastMessageId)
                      .bind(pullCount)
                      .execute();
    if (!(result.count() > 0)) {
      LOG_DEBUG(dbLogger, "Mysql指令调用的结果集为空");
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
Message::MessageGroup MysqlDao::getUserMessage(long uid, int lastMessageId,
                                               int pullCount) {
  if (lastMessageId < 0)
    return nullptr;
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> Message::
                                                                            MessageGroup {
                                                                              std::string
                                                                                  f = R"(SELECT * FROM messages WHERE receiverId = ? AND messageId >= ? ORDER BY messageId DESC LIMIT ?)";
                                                                              auto result =
                                                                                  session
                                                                                      ->sql(
                                                                                          f)
                                                                                      .bind(
                                                                                          uid)
                                                                                      .bind(
                                                                                          lastMessageId)
                                                                                      .bind(
                                                                                          pullCount)
                                                                                      .execute();
                                                                              if (!(result
                                                                                        .count() >
                                                                                    0)) {
                                                                                LOG_DEBUG(
                                                                                    dbLogger,
                                                                                    "Mysql指令调用的结果集为空");
                                                                                return nullptr;
                                                                              }
                                                                              Message::MessageGroup messageGroup(
                                                                                  new std::vector<
                                                                                      Message::
                                                                                          Ptr>());
                                                                              Message::Ptr
                                                                                  messagePtr;
                                                                              for (
                                                                                  auto
                                                                                      row :
                                                                                  result
                                                                                      .fetchAll()) {
                                                                                size_t messageId =
                                                                                    row[0]
                                                                                        .get<
                                                                                            size_t>();
                                                                                size_t fromUid =
                                                                                    row[1]
                                                                                        .get<
                                                                                            size_t>();
                                                                                size_t toUid =
                                                                                    row[2]
                                                                                        .get<
                                                                                            size_t>();
                                                                                std::string sessionKey =
                                                                                    row[3]
                                                                                        .get<
                                                                                            std::
                                                                                                string>();
                                                                                short type =
                                                                                    row[4]
                                                                                        .get<
                                                                                            int>();
                                                                                std::string content =
                                                                                    row[5]
                                                                                        .get<
                                                                                            std::
                                                                                                string>();
                                                                                short status =
                                                                                    row[6]
                                                                                        .get<
                                                                                            int>();
                                                                                std::string sendDateTime =
                                                                                    row[7]
                                                                                        .get<
                                                                                            std::
                                                                                                string>();
                                                                                std::string readDateTime =
                                                                                    row[8]
                                                                                        .get<
                                                                                            std::
                                                                                                string>();

                                                                                messagePtr
                                                                                    .reset(new Message(
                                                                                        messageId,
                                                                                        fromUid,
                                                                                        toUid,
                                                                                        std::move(
                                                                                            sessionKey),
                                                                                        type,
                                                                                        std::move(
                                                                                            content),
                                                                                        status,
                                                                                        std::move(
                                                                                            sendDateTime),
                                                                                        std::move(
                                                                                            readDateTime)));
                                                                                messageGroup
                                                                                    ->push_back(
                                                                                        messagePtr);
                                                                              }
                                                                              return messageGroup;
                                                                            });
}
int MysqlDao::deleteUser(long uid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f1 = R"(DELETE FROM users WHERE uid = ?)";
    std::string f2 = R"(DELETE FROM userInfo WHERE uid = ?)";
    auto result = session->sql(f1).bind(uid).execute();
    result = session->sql(f2).bind(uid).execute();
    return 0;
  });
}
int MysqlDao::deleteFriendApply(long fromUid, long toUid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f =
        R"(DELETE FROM friendApplys WHERE fromUid = ? AND toUid = ?)";
    auto result = session->sql(f).bind(fromUid).bind(toUid).execute();
    return 0;
  });
}
int MysqlDao::deleteFriend(long uidA, long uidB) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(DELETE FROM `friends` WHERE uidA = ? AND uidB = ?)";
    auto result = session->sql(f).bind(uidA).bind(uidB).execute();
    return 0;
  });
}

// todo...
int MysqlDao::deleteMessage(long uid, int lastMessageId, int delCount) {
  return executeTemplate(
      [&](std::unique_ptr<mysqlx::Session> &session) -> int { return 0; });
}

int MysqlDao::hasEmail(std::string email) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(SELECT 1 FROM users WHERE email = ?)";
    auto result = session->sql(f).bind(email).execute();
    return (result.fetchOne() != 0);
  });
}
int MysqlDao::hasUsername(std::string username) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(SELECT 1 FROM users WHERE username = ?)";
    auto result = session->sql(f).bind(username).execute();
    return (result.fetchOne() != 0);
  });
}

int MysqlDao::insertGroup(GroupManager::Ptr group) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(INSERT INTO `groups` VALUES (?, ?, ?, ?))";
    auto result = session->sql(f)
                      .bind(group->gid)
                      .bind(group->sessionKey)
                      .bind(group->name)
                      .bind(group->createTime)
                      .execute();
    return 0;
  });
}

int MysqlDao::insertGroupMember(GroupMember::Ptr member) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(INSERT INTO groupMembers VALUES (?, ?, ?, ?, ?, ?))";
    auto result = session->sql(f)
                      .bind(member->gid)
                      .bind(member->uid)
                      .bind(member->role)
                      .bind(member->joinTime)
                      .bind(member->speech)
                      .bind(member->memberName)
                      .execute();
    return 0;
  });
}

GroupMember::MemberList MysqlDao::getGroupList(long uid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> GroupMember::MemberList {
    std::string f = R"(SELECT * FROM groupMembers)";
    auto result = session->sql(f).execute();
    GroupMember::MemberList memberList;
    for (auto row : result.fetchAll()) {
      size_t gid = row[0].get<size_t>();
      size_t uid = row[1].get<size_t>();
      short role = row[2].get<int>();
      std::string joinTime = row[3].get<std::string>();
      short speech = row[4].get<int>();
      std::string memberName = row[5].get<std::string>();
      GroupMember::Ptr member(
          new GroupMember(gid, uid, role, joinTime, speech, memberName));
      memberList.push_back(member);
    }
    return memberList;
  });
}

int MysqlDao::hasGroup(long gid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f = R"(SELECT 1 FROM `groups` WHERE gid = ?)";
    auto result = session->sql(f).bind(gid).execute();
    return (result.fetchOne() != 0);
  });
}

GroupMember::MemberList MysqlDao::getGroupRoleMemberList(long gid, short role) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> GroupMember::MemberList {
    // role
    // 大于等于是因为，管理往上的级别按数值排序，若2为管理，3则是大于管理的级别，如群主
    std::string f = R"(SELECT * FROM groupMembers WHERE gid = ? AND role >= ?)";
    auto result = session->sql(f).bind(gid).bind(role).execute();
    GroupMember::MemberList memberList;
    for (auto row : result.fetchAll()) {
      size_t gid = row[0].get<size_t>();
      size_t uid = row[1].get<size_t>();
      short role = row[2].get<int>();
      std::string joinTime = row[3].get<std::string>();
      short speech = row[4].get<int>();
      std::string memberName = row[5].get<std::string>();
      GroupMember::Ptr member(
          new GroupMember(gid, uid, role, joinTime, speech, memberName));
      memberList.push_back(member);
    }
    return memberList;
  });
}
GroupMember::MemberList MysqlDao::getGroupMemberList(long gid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> GroupMember::MemberList {
    std::string f = R"(SELECT * FROM groupMembers WHERE gid = ?)";
    auto result = session->sql(f).bind(gid).execute();
    GroupMember::MemberList memberList;
    for (auto row : result.fetchAll()) {
      size_t gid = row[0].get<size_t>();
      size_t uid = row[1].get<size_t>();
      short role = row[2].get<int>();
      std::string joinTime = row[3].get<std::string>();
      short speech = row[4].get<int>();
      std::string memberName = row[5].get<std::string>();
      GroupMember::Ptr member(
          new GroupMember(gid, uid, role, joinTime, speech, memberName));
      memberList.push_back(member);
    }
    return memberList;
  });
}

int MysqlDao::insertGroupApply(GroupApply::Ptr apply) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) {
    std::string f =
        R"(INSERT INTO `groupApplys` VALUES (?, ?, ?, ? , ?, ?, ?))";
    auto result = session->sql(f)
                      .bind(apply->requestor)
                      .bind(apply->handler)
                      .bind(apply->gid)
                      .bind(apply->type)
                      .bind(apply->status)
                      .bind(apply->message)
                      .bind(apply->updateTime)
                      .execute();
    return 0;
  });
}

GroupApply::GroupApplyList MysqlDao::selectGroupApply() {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> GroupApply::GroupApplyList {
    std::string f = R"(SELECT * FROM `groupApplys`)";
    auto result = session->sql(f).execute();
    if (!(result.count() > 0)) {
      LOG_INFO(dbLogger, "Mysql指令调用的结果集为空");
      return nullptr;
    }
    GroupApply::GroupApplyList applyList(new std::vector<GroupApply::Ptr>());
    for (auto row : result.fetchAll()) {
      size_t requestor = row[0].get<size_t>();
      size_t handler = row[1].get<size_t>();
      size_t gid = row[2].get<size_t>();
      short type = row[3].get<int>();
      short status = row[4].get<int>();
      std::string message = row[5].get<std::string>();
      std::string updateTime = row[6].get<std::string>();
      GroupApply::Ptr apply(new GroupApply(requestor, handler, gid, type,
                                           status, message, updateTime));
      applyList->push_back(apply);
    }
    return applyList;
  });
}

int MysqlDao::hasGroupApply(long requestor, long gid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f =
        R"(SELECT 1 FROM `groupApplys` WHERE requestor = ? AND gid = ?)";
    auto result = session->sql(f).bind(requestor).bind(gid).execute();
    return (result.fetchOne() != 0);
  });
}

int MysqlDao::deleteGroupApply(long requestor, long gid) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) -> int {
    std::string f =
        R"(DELETE FROM `groupApplys` WHERE requestor = ? AND gid = ?)";
    auto result = session->sql(f).bind(requestor).bind(gid).execute();
    return 0;
  });
}

int MysqlDao::updateGroupApply(GroupApply::Ptr apply) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session) {
    std::string f =
        R"(UPDATE `groupApplys` SET handler = ?, status = ?, type = ?, message = ?, updateTime = ? WHERE requestor = ? AND gid = ?)";
    auto result = session->sql(f)
                      .bind(apply->handler)
                      .bind(apply->status)
                      .bind(apply->type)
                      .bind(apply->message)
                      .bind(apply->updateTime)
                      .bind(apply->requestor)
                      .bind(apply->gid)
                      .execute();
    return 0;
  });
}

GroupApply::GroupApplyList MysqlDao::pullGroupApply(long requestor) {
  return executeTemplate([&](std::unique_ptr<mysqlx::Session> &session)
                             -> GroupApply::GroupApplyList {
    std::string f = R"(SELECT * FROM `groupApplys` WHERE requestor = ?)";
    auto result = session->sql(f).bind(requestor).execute();
    if (!(result.count() > 0)) {
      LOG_INFO(dbLogger, "Mysql指令调用的结果集为空");
      return nullptr;
    }
    GroupApply::GroupApplyList applyList(new std::vector<GroupApply::Ptr>());
    for (auto row : result.fetchAll()) {
      size_t requestor = row[0].get<size_t>();
      size_t handler = row[1].get<size_t>();
      size_t gid = row[2].get<size_t>();
      short type = row[3].get<int>();
      short status = row[4].get<int>();
      std::string message = row[5].get<std::string>();
      std::string updateTime = row[6].get<std::string>();
      GroupApply::Ptr apply(new GroupApply(requestor, handler, gid, type,
                                           status, message, updateTime));
      applyList->push_back(apply);
    }
    return applyList;
  });
}
}; // namespace wim::db