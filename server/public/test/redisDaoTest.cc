#include "Redis.h"
#include <gtest/gtest.h>

TEST(RedisDao, test) { ASSERT_EQ(1, 1); }

int main() {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}