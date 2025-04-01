#include "MysqlOperator.h"
#include "Configer.h"
#include "Const.h"
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <spdlog/spdlog.h>

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

int MysqlOperator::RegisterUser(const std::string &name,
                                const std::string &email,
                                const std::string &pwd) {
  auto con = pool->getConnection();

  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool->returnConnection(std::move(con)); });

  try {
    std::unique_ptr<sql::PreparedStatement> stmt(
        con->sql->prepareStatement("CALL UserRegister(?,?,?,@result)"));
    stmt->setString(1, email);
    stmt->setString(2, name);
    stmt->setString(3, pwd);

    stmt->execute();
    std::unique_ptr<sql::Statement> stmtResult(con->sql->createStatement());
    std::unique_ptr<sql::ResultSet> res(
        stmtResult->executeQuery("SELECT @result AS result"));
    if (res->next()) {
      int result = res->getInt("result");
      spdlog::info("[sql-UserRegister]-Result: {}", result);
      return result;
    }
    return -1;
  } catch (sql::SQLException &e) {
    spdlog::error("SQLException: {} (MySQL error code: {}, SQLState: {})",
                  e.what(), e.getErrorCode(), e.getSQLState());

    return -1;
  }
}

bool MysqlOperator::CheckEmail(const std::string &name,
                               const std::string &email) {
  auto con = pool->getConnection();

  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool->returnConnection(std::move(con)); });

  try {
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->sql->prepareStatement("CALL CheckEmail(?)"));

    pstmt->setString(1, name);

    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

    while (res->next()) {
      spdlog::info("Email: {}", res->getString("email").asStdString());
      if (email != res->getString("email")) {
        return false;
      }
      return true;
    }
    return false;
  } catch (sql::SQLException &e) {
    spdlog::error("SQLException: {} (MySQL error code: {}, SQLState: {})",
                  e.what(), e.getErrorCode(), e.getSQLState());

    return false;
  }
}

bool MysqlOperator::UpdatePassword(const std::string &name,
                                   const std::string &newpwd) {
  auto con = pool->getConnection();

  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool->returnConnection(std::move(con)); });

  try {
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->sql->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

    pstmt->setString(2, name);
    pstmt->setString(1, newpwd);

    int updateCount = pstmt->executeUpdate();

    std::cout << "Updated rows: " << updateCount << "\n";
    return true;
  } catch (sql::SQLException &e) {
    spdlog::error("SQLException: {} (MySQL error code: {}, SQLState: {})",
                  e.what(), e.getErrorCode(), e.getSQLState());

    return false;
  }
}

bool MysqlOperator::CheckUserExist(const std::string &email,
                                   const std::string &password,
                                   UserInfo &userInfo) {
  auto con = pool->getConnection();
  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool->returnConnection(std::move(con)); });

  try {

    con->sql->setAutoCommit(true);
    std::unique_ptr<sql::PreparedStatement> stmt(
        con->sql->prepareStatement("CALL GetUserByEmail(?)"));
    stmt->setString(1, email);

    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

    if (!res->next()) {
      spdlog::info("[CheckUserExist]: no such email");
      return false;
    }

    auto originPassword = res->getString("password");
    if (originPassword != password) {
      spdlog::info("[CheckUserExist] originPassword != password!");
      return false;
    }

    userInfo.uid = res->getInt("uid");
    // userInfo.name = res->getString("name");
    userInfo.email = res->getString("email");
    userInfo.password = originPassword;

    return true;
  } catch (sql::SQLException &e) {
    spdlog::error("SQLException: {} (MySQL error code: {}, SQLState: {})",
                  e.what(), e.getErrorCode(), e.getSQLState());
    return false;
  }
}