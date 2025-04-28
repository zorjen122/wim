#include <Redis.h>

int main() {
  sw::redis::Redis redis("tcp://127.0.0.1:6380");
  auto reply = redis.set("key", "value");
  auto reply = redis.get("key");
}