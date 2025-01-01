#include "util.h"

#include <cppconn/connection.h>
#include <ctime>
#include <iostream>
#include <random>
#include <string>

using namespace std;

// 生成一个随机字符串作为用户名
string generateRandomUserName(int length) {
  const string chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  string result;
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, chars.size() - 1);

  for (int i = 0; i < length; ++i) {
    result += chars[dis(gen)];
  }
  return result;
}

// 生成随机电子邮件（Gmail 或 QQ）
string generateRandomEmail() {
  // 随机选择邮箱的域名：Gmail 或 QQ
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, 1); // 0 -> Gmail, 1 -> QQ
  int domainChoice = dis(gen);

  string domain;
  if (domainChoice == 0) {
    domain = "@gmail.com";
  } else {
    domain = "@qq.com";
  }

  // 随机生成用户名，长度为8-12个字符
  int usernameLength = rand() % 5 + 8; // 生成一个8到12之间的随机长度
  string username = generateRandomUserName(usernameLength);

  return username + domain;
}

#include <cppconn/resultset.h>
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/statement.h>
#include <spdlog/spdlog.h>

void fetchUsersFromDatabase(UserManager *users) {
  std::unique_ptr<sql::mysql::MySQL_Driver> driver;

  try {
    // 创建连接
    driver.reset(sql::mysql::get_mysql_driver_instance());
    std::unique_ptr<sql::Connection> con(
        driver->connect("tcp://127.0.0.1:3309", "zorjen", "root"));
    // 选择数据库
    con->setSchema("chatServ");

    std::unique_ptr<sql::Statement> stmtResult(con->createStatement());
    // 执行查询
    std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery(
        "SELECT id, uid, name, email, password FROM users"));

    bool isEmpty = 0;
 
    while (res->next()) {
      isEmpty = true;
      User user;
      user.id = res->getInt("id");
      user.uid = res->getInt("uid");
      user.username = res->getString("name");
      user.email = res->getString("email");
      user.password = res->getString("password");
      users->push_back(user);
    }

    if(!isEmpty)
    {
      spdlog::error("No such select users");
      return;
    }

  } catch (sql::SQLException &e) {
    std::cerr << "Error fetching users from database: " << e.what()
              << std::endl;
  }

}


int generateRandomNumber(int bound)
{
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, bound);

  return dis(gen);
}void toNormalString(std::string &str) {
  // 去掉前后的换行符、空格、制表符等

  auto first = str.find_first_of("\"");
  auto last = str.find_last_of("\"");
  if (first == std::string::npos || last == std::string::npos)
    return;

  str = str.substr(first + 1, last - 1);
}
