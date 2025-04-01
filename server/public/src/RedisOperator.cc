#include "RedisOperator.h"
#include "Configer.h"
#include "Const.h"
#include "hiredis.h"
#include "spdlog/spdlog.h"

RedisOperator::RedisOperator() {
  auto conf = Configer::getConfig("server");
  auto host = conf["redis"]["host"].as<std::string>();
  auto port = conf["redis"]["port"].as<std::string>();
  auto passwd = conf["redis"]["password"].as<std::string>();
  pool.reset(new RedisPool(5, host, std::stoi(port), passwd));
}

RedisOperator::~RedisOperator() {
  spdlog::info("RedisManager::~RedisManager()");
}

bool RedisOperator::Get(const std::string &key, std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }

  auto reply = (redisReply *)redisCommand(connect, "GET %s", key.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });

  if (reply == NULL) {
    spdlog::info("[RedisManager::Get] get key {} is failed", key);
    return false;
  }

  if (reply->type != REDIS_REPLY_STRING) {
    spdlog::info("[RedisManager::Get] get key {} is failed", key);
    return false;
  }

  value = reply->str;

  spdlog::info("[RedisManager::Get] key:{}, value:{}", key, value);
  return true;
}

bool RedisOperator::Set(const std::string &key, const std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "SET %s %s", key.c_str(),
                                          value.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });

  if (NULL == reply) {
    spdlog::info("[RedisManager::Set] set key {} is failed", key);
    return false;
  }

  Defer defer([reply] { freeReplyObject(reply); });
  if (!(reply->type == REDIS_REPLY_STATUS &&
        (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0))) {
    spdlog::info("[RedisManager::Set] set key {} is failed", key);
    return false;
  }

  spdlog::info("[RedisManager::Set] key:{}, value:{}", key, value);
  return true;
}

bool RedisOperator::LPush(const std::string &key, const std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "LPUSH %s %s", key.c_str(),
                                          value.c_str());
  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });

  if (NULL == reply) {
    spdlog::info("[RedisManager::LPush] set key {} is failed", key);

    return false;
  }

  if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
    spdlog::info("[RedisManager::LPush] set key {} is failed", key);

    return false;
  }

  spdlog::info("[RedisManager::LPush] key:{}, value:{}", key, value);
  freeReplyObject(reply);
  pool->ReturnConnection(connect);
  return true;
}

bool RedisOperator::LPop(const std::string &key, std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "LPOP %s ", key.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::info("[RedisManager::LPop] get key {} is failed", key);
    return false;
  }

  if (reply->type == REDIS_REPLY_NIL) {
    spdlog::info("[RedisManager::LPop] get key {} is failed", key);
    return false;
  }

  value = reply->str;
  spdlog::info("[RedisManager::LPop] key:{}, value:{}", key, value);
  return true;
}

bool RedisOperator::RPush(const std::string &key, const std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "RPUSH %s %s", key.c_str(),
                                          value.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (NULL == reply) {
    spdlog::info("[RedisManager::RPush] set key {} is failed", key);
    return false;
  }

  if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
    spdlog::info("[RedisManager::RPush] set key {} is failed", key);
    return false;
  }

  spdlog::info("[RedisManager::RPush] key:{}, value:{}", key, value);
  return true;
}
bool RedisOperator::RPop(const std::string &key, std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "RPOP %s ", key.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::warn("[RedisManager::RPop] get key {} is failed", key);
    return false;
  }

  if (reply->type == REDIS_REPLY_NIL) {
    spdlog::warn("[RedisManager::RPop] get key {} is failed", key);
    return false;
  }
  value = reply->str;

  spdlog::info("[RedisManager::RPop] key:{}, value:{}", key, value);
  freeReplyObject(reply);
  pool->ReturnConnection(connect);
  return true;
}

bool RedisOperator::HSet(const std::string &key, const std::string &hkey,
                         const std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "HSET %s %s %s", key.c_str(),
                                          hkey.c_str(), value.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::warn("[RedisManager::HSet] set key {} is failed", key);
    return false;
  }

  if (reply->type != REDIS_REPLY_INTEGER) {
    spdlog::warn("[RedisManager::HSet] set key {} is failed", key);
    return false;
  }

  spdlog::info("[RedisManager::HSet] key:{}, hkey:{}, value:{}", key, hkey,
               value);
  return true;
}

bool RedisOperator::HSet(const char *key, const char *hkey, const char *hvalue,
                         size_t hvaluelen) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  const char *argv[4];
  size_t argvlen[4];
  argv[0] = "HSET";
  argvlen[0] = 4;
  argv[1] = key;
  argvlen[1] = strlen(key);
  argv[2] = hkey;
  argvlen[2] = strlen(hkey);
  argv[3] = hvalue;
  argvlen[3] = hvaluelen;

  auto reply = (redisReply *)redisCommandArgv(connect, 4, argv, argvlen);

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });

  if (reply == nullptr) {
    spdlog::warn("[RedisManager::HSet] set key {} is failed", key);
    return false;
  }

  if (reply->type != REDIS_REPLY_INTEGER) {
    spdlog::warn("[RedisManager::HSet] set key {} is failed", key);
    return false;
  }
  spdlog::info("[RedisManager::HSet] key:{}, hkey:{}, hvalue:{}", key, hkey,
               hvalue);
  return true;
}

std::string RedisOperator::HGet(const std::string &key,
                                const std::string &hkey) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return "";
  }
  const char *argv[3];
  size_t argvlen[3];
  argv[0] = "HGET";
  argvlen[0] = 4;
  argv[1] = key.c_str();
  argvlen[1] = key.length();
  argv[2] = hkey.c_str();
  argvlen[2] = hkey.length();

  auto reply = (redisReply *)redisCommandArgv(connect, 3, argv, argvlen);

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::warn("[RedisManager::HGet] get key {} is failed", key);
    return "";
  }

  if (reply->type == REDIS_REPLY_NIL) {
    spdlog::warn("[RedisManager::HGet] get key {} is failed", key);
    return "";
  }

  std::string value = reply->str;
  spdlog::info("[RedisManager::HGet] key:{}, hkey:{}, value:{}", key, hkey,
               value);

  return value;
}

bool RedisOperator::HDel(const std::string &key, const std::string &field) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }

  Defer defer([&connect, this]() { pool->ReturnConnection(connect); });

  redisReply *reply = (redisReply *)redisCommand(connect, "HDEL %s %s",
                                                 key.c_str(), field.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });

  if (reply == nullptr) {
    spdlog::warn("RedisManager::HDel] HDEL command failed");
    return false;
  }

  bool success = false;
  if (reply->type == REDIS_REPLY_INTEGER) {
    success = reply->integer > 0;
  }

  spdlog::info("[RedisManager::HDel] key:{}, field:{}", key, field);
  return success;
}

bool RedisOperator::Del(const std::string &key) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }
  auto reply = (redisReply *)redisCommand(connect, "DEL %s", key.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::warn("[RedisManager::Del] DEL command failed");
    return false;
  }

  if (reply->type != REDIS_REPLY_INTEGER) {
    spdlog::warn("[RedisManager::Del] DEL command failed");
    return false;
  }

  spdlog::info("[RedisManager::Del] success! key:{}", key);
  freeReplyObject(reply);
  pool->ReturnConnection(connect);
  return true;
}

bool RedisOperator::ExistsKey(const std::string &key) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }

  auto reply = (redisReply *)redisCommand(connect, "exists %s", key.c_str());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::warn("[RedisManager::ExistsKey] EXISTS command failed");
    return false;
  }

  if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
    spdlog::warn("[RedisManager::ExistsKey] EXISTS command failed");
    return false;
  }
  spdlog::info("[RedisManager::ExistsKey] success! key:{}", key);
  return true;
}

void RedisOperator::Close() {
  pool->Close();
  pool->ClearConnections();
}

bool RedisOperator::SetExpire(const std::string &key, int seconds,
                              const std::string &value) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }

  auto reply = (redisReply *)redisCommand(connect, "SETEX %b %d %b", key.data(),
                                          key.size(), seconds, value.data(),
                                          value.size());

  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });

  if (reply == nullptr) {
    spdlog::warn("[RedisManager::ExistsKey] EXISTS command failed");
    return false;
  }
  return true;
}

bool RedisOperator::Expire(const std::string &key, int seconds) {
  auto connect = pool->GetConnection();
  if (connect == nullptr) {
    return false;
  }

  auto reply = (redisReply *)redisCommand(connect, "EXPIRE %b %d", key.data(),
                                          key.size(), seconds);
  Defer _([this, reply, connect]() {
    freeReplyObject(reply);
    pool->ReturnConnection(connect);
  });
  if (reply == nullptr) {
    spdlog::warn("[RedisManager::ExistsKey] EXISTS command failed");
    return false;
  }
  return true;
}
