#pragma once
#include <thread>

#include "Const.h"
#include "MysqlPool.h"

struct UserInfo {
  std::string name;
  std::string password;
  int uid;
  std::string email;
};

class MysqlOperator : public Singleton<MysqlOperator> {
public:
  MysqlOperator();
  ~MysqlOperator();
  int RegisterUser(const std::string &name, const std::string &email,
                   const std::string &pwd);

  bool CheckEmail(const std::string &name, const std::string &email);
  bool UpdatePassword(const std::string &name, const std::string &newpwd);
  bool CheckUserExist(const std::string &name, const std::string &pwd,
                      UserInfo &userInfo);

private:
  std::unique_ptr<MySqlPool> pool;
};