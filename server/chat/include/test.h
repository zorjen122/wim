#pragma once

// #define TEST_IM

#include <string>
#include <unordered_map>
#include <vector>

#include "ChatSession.h"

namespace __test {
static std::unordered_map<int, std::shared_ptr<ChatSession>> idMap;
static std::vector<int> idGroup;

enum { TEST_PUSH_ID = 939 };
static int sessionId = 0;

};  // namespace __test