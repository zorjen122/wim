#include "SqlOperator.h"
#include "Configer.h"
#include "Const.h"

MySqlOperator::MySqlOperator(const std::string &host, const std::string &port,
                             const std::string &user,
                             const std::string &password,
                             const std::string &schema, size_t poolSize) {
  pool.reset(
      new MySqlPool(host + ":" + port, user, password, schema, poolSize));
}

MySqlOperator::MySqlOperator() {
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

MySqlOperator::~MySqlOperator() { pool->close(); }

bool MySqlOperator::SaveService(size_t from, size_t to,
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
      spdlog::warn("[MySqlOperator::SaveService] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MySqlOperator::SaveService] Exception occurred: {}",
                  e.what());
    return false;
  } catch (...) {
    spdlog::error("[MySqlOperator::SaveService] Unknown exception occurred.");
    return false;
  }
}

// todo... return user info type
bool MySqlOperator::UserSearch(size_t uid) {
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
      spdlog::warn("[MySqlOperator::UserSerach] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MySqlOperator::UserSerach] Exception occurred: {}",
                  e.what());
    return false;
  }
}

bool MySqlOperator::AppendPair(size_t uidA, size_t uidB) {
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
      spdlog::warn("[MySqlOperator::AppendPair] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MySqlOperator::AppendPair] Exception occurred: {}",
                  e.what());
    return false;
  }
}

bool MySqlOperator::RemovePair(size_t uidA, size_t uidB) {
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
      spdlog::warn("[MySqlOperator::RemovePair] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[MySqlOperator::RemovePair] Exception occurred: {}",
                  e.what());
    return false;
  }
}

// todo... return pair info type
bool MySqlOperator::PairSearch(size_t uidA, size_t uidB) {
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
      spdlog::warn("[MySqlOperator::PairSearch] No result returned from the "
                   "stored procedure.");
      return false;
    }
  } catch (const std::exception &e) { // 捕获所有异常
    spdlog::error("[MySqlOperator::PairSearch] Exception occurred: {}",
                  e.what());
    return false;
  }
}