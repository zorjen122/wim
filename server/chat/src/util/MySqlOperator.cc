#include "MysqlOperator.h"
#include "Configer.h"
#include "Const.h"

MysqlOperator::MysqlOperator(const std::string &host, const std::string &port,
                             const std::string &user,
                             const std::string &password,
                             const std::string &schema, size_t poolSize) {
  pool.reset(
      new MySqlPool(host + ":" + port, user, password, schema, poolSize));
}

MysqlOperator::MysqlOperator() {
  auto conf = Configer::getConfig("server");

  if (conf.IsNull())
    spdlog::error("Configer::getConfig(\"Server\") failed");

  auto host = conf["mysql"]["host"].as<std::string>();
  auto port = conf["mysql"]["port"].as<unsigned short>();
  auto user = conf["mysql"]["user"].as<std::string>();
  auto passwd = conf["mysql"]["password"].as<std::string>();
  auto schema = conf["mysql"]["schema"].as<std::string>();

  pool.reset(new MySqlPool(host + ":" + std::to_string(port), user, passwd,
                           schema, 5));
}

MysqlOperator::~MysqlOperator() { pool->close(); }

bool MysqlOperator::SaveService(size_t from, size_t to,
                                const std::string &context) {
  auto con = pool->getConnection();
  Defer _([&con, this]() { pool->returnConnection(std::move(con)); });
  try {
    auto cmd = con->sql->prepareStatement("CALL SaveService(?, ?, ?)");
    cmd->setUInt(0, from);
    cmd->setUInt(1, to);
    cmd->setString(2, context);

    auto result = cmd->executeQuery();

    if (result->next()) {
      bool success = result->getInt(0); // 获取第一列的值，假设返回一个布尔值
      return success;
    } else {
      spdlog::warn("[MysqlOperator::SaveService] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MysqlOperator::SaveService] Exception occurred: {}",
                  e.what());
    return false;
  } catch (...) {
    spdlog::error("[MysqlOperator::SaveService] Unknown exception occurred.");
    return false;
  }
}

// todo... return user info type
bool MysqlOperator::UserSearch(size_t uid) {
  auto con = pool->getConnection();
  Defer _([&con, this]() { pool->returnConnection(std::move(con)); });
  try {
    auto cmd = con->sql->prepareStatement("CALL UserSerach(?)");
    cmd->setUInt(0, uid);
    auto result = cmd->executeQuery();
    if (result->next()) {
      bool success = result->getInt(0); // 获取第一列的值，假设返回一个布尔值
      return success;
    } else {
      spdlog::warn("[MysqlOperator::UserSerach] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MysqlOperator::UserSerach] Exception occurred: {}",
                  e.what());
    return false;
  }
}

bool MysqlOperator::AppendPair(size_t uidA, size_t uidB) {
  auto con = pool->getConnection();
  Defer _([&con, this]() { pool->returnConnection(std::move(con)); });
  try {
    auto cmd = con->sql->prepareStatement("CALL AppendPair(?, ?)");
    cmd->setUInt(0, uidA);
    cmd->setUInt(1, uidB);
    auto result = cmd->executeQuery();
    if (result->next()) {
      bool success = result->getInt(0); // 获取第一列的值，假设返回一个布尔值
      return success;
    } else {
      spdlog::warn("[MysqlOperator::AppendPair] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MysqlOperator::AppendPair] Exception occurred: {}",
                  e.what());
    return false;
  }
}

bool MysqlOperator::RemovePair(size_t uidA, size_t uidB) {
  auto con = pool->getConnection();
  Defer _([&con, this]() { pool->returnConnection(std::move(con)); });
  try {
    auto cmd = con->sql->prepareStatement("CALL RemovePair(?, ?)");
    cmd->setUInt(0, uidA);
    cmd->setUInt(1, uidB);
    auto result = cmd->executeQuery();
    if (result->next()) {
      bool success = result->getInt(0); // 获取第一列的值，假设返回一个布尔值
      return success;
    } else {
      spdlog::warn("[MysqlOperator::RemovePair] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MysqlOperator::RemovePair] Exception occurred: {}",
                  e.what());
    return false;
  }
}

// todo... return pair info type
bool MysqlOperator::PairSearch(size_t uidA, size_t uidB) {
  auto con = pool->getConnection();
  Defer _([&con, this]() { pool->returnConnection(std::move(con)); });
  try {
    auto cmd = con->sql->prepareStatement("CALL PairSearch(?, ?)");
    cmd->setUInt(0, uidA);
    cmd->setUInt(1, uidB);
    auto result = cmd->executeQuery();
    if (result->next()) {
      bool success = result->getInt(0); // 获取第一列的值，假设返回一个布尔值
      return success;
    } else {
      spdlog::warn("[MysqlOperator::PairSearch] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) { // 捕获所有异常
    spdlog::error("[MysqlOperator::PairSearch] Exception occurred: {}",
                  e.what());
    return false;
  }
}