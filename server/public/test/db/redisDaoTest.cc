#include "Logger.h"
#include "Redis.h"

namespace wim::db {
void test() {
  RedisDao::GetInstance();
  LOG_DEBUG(dbLogger, "Hello, RedisDao");
}
} // namespace wim::db

int main() {
  Configer::loadConfig("../config.yaml");
  wim::db::test();
  return 0;
}