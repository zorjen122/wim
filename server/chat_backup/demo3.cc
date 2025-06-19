#include <iostream>
#include <sw/redis++/redis.h>

int main() {
  sw::redis::Redis redis("tcp://127.0.0.1:6380");
  redis.auth("root");
  redis.set("key", "value");
  // redis.del("key");

  auto ret = redis.get("key");

  std::cout << (ret.has_value() ? "key exists" : "key does not exist")
            << std::endl;

  return 0;
}