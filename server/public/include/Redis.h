#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <jsoncpp/json/json.h>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <sw/redis++/redis.h>
#include <thread>
#include <yaml-cpp/yaml.h>

#include "Const.h"
#include "DbGlobal.h"

namespace wim::db {

class RedisPool {
public:
  using Ptr = std::shared_ptr<RedisPool>;

  RedisPool(const std::string &host, unsigned int port,
            const std::string &password, size_t poolSize = 2);
  ~RedisPool();

  void ClearConnections();
  std::unique_ptr<sw::redis::Redis> GetConnection();
  void ReturnConnection(std::unique_ptr<sw::redis::Redis> context);
  void Close();
  bool Empty();
  size_t Size();

private:
  void keepConnectionHandle();

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
                 public std::enable_shared_from_this<RedisDao> {
  friend class TestDb;

public:
  using Ptr = std::shared_ptr<RedisDao>;

  RedisDao(const std::string &host, unsigned short port,
           const std::string &password, size_t clientCount, size_t machine);
  RedisDao();
  RedisDao(YAML::Node &conf);

  template <typename Func>
  auto executeTemplate(Func &&processor) -> decltype(
      processor(std::declval<std::unique_ptr<sw::redis::Redis> &>()));

  template <typename T> static auto defaultForType();

  std::string echo(const std::string &content);
  void Close();

  const std::string PrefixUserId = "im:userId:";
  std::string getPrefixUserId();

  const std::string PrefixOnlineUserInfo = "im:user:";
  std::string getPrefixOnlineUserInfo();

  int64_t generateUserId();
  int64_t generateMsgId();
  int64_t generateSessionId();
  int64_t generateGid();

  bool setOnlineUser(const std::string &device, long userId,
                     std::string machineId);
  std::string getOnlineUser(const std::string &device, long userId);
  bool delOnlineUser(const std::string &device, long userId);
  bool hasOnlineUser(const std::string &device, long userId);

  std::string getMsgId(int64_t msgId);

  bool setUserMsgId(long userId, int64_t msgId, short expired);
  bool getUserMsgId(long userId, int64_t msgId);
  bool expireUserMsgId(long userId, int64_t msgId, short expired);

  bool authVerifycode(const std::string &email, const std::string &verifycode);

  bool setOnlineUserInfo(db::UserInfo::Ptr userInfo, std::string machineId);
  std::string getOnlineUserInfo(long uid);
  Json::Value getOnlineUserInfoObject(long uid);
  bool delOnlineUserInfo(long uid);

private:
  void build(const std::string &host, unsigned short port,
             const std::string &password, size_t clientCount, size_t machine);
  int64_t generateId(const std::string &key);

  RedisPool::Ptr redisPool;
  uint16_t machineId;
  static const int64_t epoch = 1672531200000;
};

} // namespace wim::db