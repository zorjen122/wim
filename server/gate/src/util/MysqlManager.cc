#include "MysqlManager.h"

#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <spdlog/spdlog.h>

#include "Configer.h"
#include "Const.h"
MysqlManager::MysqlManager() {
  auto &cfg = Configer::GetInstance();
  const auto &host = cfg["Mysql"]["Host"];
  const auto &port = cfg["Mysql"]["Port"];
  const auto &pwd = cfg["Mysql"]["Passwd"];
  const auto &schema = cfg["Mysql"]["Schema"];
  const auto &user = cfg["Mysql"]["User"];
  pool_.reset(
      new MySqlPool("tcp://" + host + ":" + port, user, pwd, schema, 5));
  std::cout << "mysql-pool url is: " << host + ":" + port << "\n";
}

MysqlManager::~MysqlManager() { pool_->Close(); }

int MysqlManager::RegisterUser(const std::string &name,
                               const std::string &email,
                               const std::string &pwd) {
  auto con = pool_->getConnection();

  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool_->returnConnection(std::move(con)); });

  try {
    // 准锟斤拷锟斤拷锟矫存储锟斤拷锟斤拷
    std::unique_ptr<sql::PreparedStatement> stmt(
        con->_con->prepareStatement("CALL UserRegister(?,?,?,@result)"));
    stmt->setString(1, email);
    stmt->setString(2, name);
    stmt->setString(3, pwd);

    stmt->execute();
    std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
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

bool MysqlManager::CheckEmail(const std::string &name,
                              const std::string &email) {
  auto con = pool_->getConnection();

  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool_->returnConnection(std::move(con)); });

  try {
    // 准锟斤拷锟斤拷询锟斤拷锟�
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->_con->prepareStatement("CALL CheckEmail(?)"));

    // 锟襟定诧拷锟斤拷
    pstmt->setString(1, name);

    // 执锟叫诧拷询
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

    // 锟斤拷锟斤拷锟斤拷锟斤拷锟�
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

bool MysqlManager::UpdatePassword(const std::string &name,
                                  const std::string &newpwd) {
  auto con = pool_->getConnection();

  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool_->returnConnection(std::move(con)); });

  try {
    // 准锟斤拷锟斤拷询锟斤拷锟�
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

    // 锟襟定诧拷锟斤拷
    pstmt->setString(2, name);
    pstmt->setString(1, newpwd);

    // 执锟叫革拷锟斤拷
    int updateCount = pstmt->executeUpdate();

    std::cout << "Updated rows: " << updateCount << "\n";
    return true;
  } catch (sql::SQLException &e) {
    spdlog::error("SQLException: {} (MySQL error code: {}, SQLState: {})",
                  e.what(), e.getErrorCode(), e.getSQLState());

    return false;
  }
}

bool MysqlManager::CheckUserExist(const std::string &email,
                                  const std::string &password,
                                  UserInfo &userInfo) {
  auto con = pool_->getConnection();
  if (con == nullptr) {
    return false;
  }
  Defer defer([&con, this]() { pool_->returnConnection(std::move(con)); });

  try {

    con->_con->setAutoCommit(true);
    std::unique_ptr<sql::PreparedStatement> stmt(
        con->_con->prepareStatement("CALL GetUserByEmail(?)"));
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
