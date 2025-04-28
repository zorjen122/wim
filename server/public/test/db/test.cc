#include "Mysql.h"
#include "Redis.h"
#include <iostream>
#include <utility>

namespace wim::db {
class TestDb {
public:
  static void Test() {
    auto sql = MysqlDao::GetInstance();
    auto redis = RedisDao::GetInstance();

    std::cout << "mysql size: " << sql->mysqlPool->Size() << "\n";
    auto con = sql->mysqlPool->GetConnection();
    std::cout << "mysql size: " << sql->mysqlPool->Size() << "\n";
    sql->mysqlPool->ReturnConnection(std::move(con));
    std::cout << "mysql size: " << sql->mysqlPool->Size() << "\n";

    std::cout << "redis size: " << redis->redisPool->Size() << "\n";
    auto rdCon = redis->redisPool->GetConnection();
    std::cout << "redis size: " << redis->redisPool->Size() << "\n";
    redis->redisPool->ReturnConnection(std::move(rdCon));
    std::cout << "redis size: " << redis->redisPool->Size() << "\n";
  };
};
}; // namespace wim::db

int main() {
  Configer::loadConfig("../config.yaml");

  wim::db::TestDb::Test();
  return 0;
  // auto sql = wim::db::MysqlDao::GetInstance();
}