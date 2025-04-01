#pragma once
#include "MysqlPool.h"
class MysqlOperator : public Singleton<MysqlOperator> {
  friend class Singleton<MysqlOperator>;

public:
  MysqlOperator(const std::string &host, const std::string &port,
                const std::string &user, const std::string &password,
                const std::string &schema, size_t poolSize = 5);
  MysqlOperator();
  ~MysqlOperator();

public:
  bool SaveService(size_t from, size_t to, const std::string &context);
  bool UserSearch(size_t uid);

  bool AppendPair(size_t uidA, size_t uidB);
  bool RemovePair(size_t uidA, size_t uidB);
  bool PairSearch(size_t uidA, size_t uidB);

private:
  std::unique_ptr<MySqlPool> pool;
};
