#include "MysqlManager.h"
#include "Configer.h"

MysqlManager::MysqlManager(const std::string &host, const std::string &port,
                           const std::string &user, const std::string &password,
                           const std::string &schema, size_t poolSize) {
  pool_.reset(
      new MySqlPool(host + ":" + port, user, password, schema, poolSize));
}

MysqlManager::MysqlManager() {
  auto conf = Configer::getConfig("server");

  if (conf.IsNull())
    spdlog::error("Configer::getConfig(\"Server\") failed");

  auto host = conf["mysql"]["host"].as<std::string>();
  auto port = conf["mysql"]["port"].as<unsigned short>();
  auto user = conf["mysql"]["user"].as<std::string>();
  auto passwd = conf["mysql"]["password"].as<std::string>();
  auto schema = conf["mysql"]["schema"].as<std::string>();

  pool_.reset(new MySqlPool(host + ":" + std::to_string(port), user, passwd,
                            schema, 5));
}

MysqlManager::~MysqlManager() { pool_->Close(); }