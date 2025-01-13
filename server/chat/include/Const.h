#pragma once
#include <functional>

enum ErrorCodes {
  Success = 0,
  Error_Json = 1001,     // Json????????
  RPCFailed = 1002,      // RPC???????
  VarifyExpired = 1003,  // ????????
  VarifyCodeErr = 1004,  // ????????
  UserExist = 1005,      // ??????????
  PasswdErr = 1006,      // ???????
  EmailNotMatch = 1007,  // ???????
  PasswdUpFailed = 1008, // ???????????
  PasswdInvalid = 1009,  // ??????????
  TokenInvalid = 1010,   // Token?
  UidInvalid = 1011,     // uid??
};

#include <memory>
#include <mutex>
template <typename T> class Singleton {
protected:
  Singleton() = default;
  Singleton(const Singleton<T> &) = delete;
  Singleton &operator=(const Singleton<T> &st) = delete;

public:
  static std::shared_ptr<T> GetInstance() {
    static std::shared_ptr<T> instance;
    static std::once_flag s_flag;
    std::call_once(s_flag, [&]() { instance = std::shared_ptr<T>(new T); });
    return instance;
  }
  ~Singleton() {}
};
class Defer {
public:
  Defer(std::function<void()> func) : func_(func) {}

  ~Defer() { func_(); }

private:
  std::function<void()> func_;
};

#define PACKAGE_MAX_LENGTH 1024 * 2
#define PACKAGE_TOTAL_LEN 4
#define PACKAGE_ID_LEN 2
#define PACKAGE_DATA_LEN 2
#define MAX_RECV_QUEUE_LEN 10000
#define MAX_SEND_QUEUE_LEN 1000

#include <string>
#include <unordered_map>

enum ServiceID {
  ID_ONLINE_PULL_REQ = 1001,
  ID_ONLINE_PULL_RSP = 1002,
  ID_PING_PONG_REQ = 1003,
  ID_PING_PONG_RSP = 1004,
  ID_CHAT_LOGIN_INIT = 1005,
  ID_CHAT_LOGIN_INIT_RSP = 1006,
  ID_SEARCH_USER_REQ = 1007,
  ID_SEARCH_USER_RSP = 1008,
  ID_ADD_FRIEND_REQ = 1009,
  ID_ADD_FRIEND_RSP = 1010,
  ID_NOTIFY_ADD_FRIEND_REQ = 1011,
  ID_AUTH_FRIEND_REQ = 1013,
  ID_AUTH_FRIEND_RSP = 1014,
  ID_NOTIFY_AUTH_FRIEND_REQ = 1015,
  ID_PUSH_TEXT_MSG_REQ = 1017,
  ID_TEXT_CHAT_MSG_RSP = 1018,
  ID_NOTIFY_PUSH_TEXT_MSG_REQ = 1019,
};

// 建立服务ID到描述的映射
static std::unordered_map<ServiceID, std::string> __serviceIdMap = {
    {ID_ONLINE_PULL_REQ, "Online Pull Request"},
    {ID_ONLINE_PULL_RSP, "Online Pull Response"},
    {ID_PING_PONG_REQ, "Ping Request"},
    {ID_PING_PONG_RSP, "Pong Response"},
    {ID_CHAT_LOGIN_INIT, "Chat Login Init"},
    {ID_CHAT_LOGIN_INIT_RSP, "Chat Login Init Response"},
    {ID_SEARCH_USER_REQ, "Search User Request"},
    {ID_SEARCH_USER_RSP, "Search User Response"},
    {ID_ADD_FRIEND_REQ, "Add Friend Request"},
    {ID_ADD_FRIEND_RSP, "Add Friend Response"},
    {ID_NOTIFY_ADD_FRIEND_REQ, "Notify Add Friend Request"},
    {ID_AUTH_FRIEND_REQ, "Auth Friend Request"},
    {ID_AUTH_FRIEND_RSP, "Auth Friend Response"},
    {ID_NOTIFY_AUTH_FRIEND_REQ, "Notify Auth Friend Request"},
    {ID_PUSH_TEXT_MSG_REQ, "Push Text Message Request"},
    {ID_TEXT_CHAT_MSG_RSP, "Text Chat Message Response"},
    {ID_NOTIFY_PUSH_TEXT_MSG_REQ, "Notify Push Text Message Request"},
};

// 建立服务ID到存在性的映射
static std::unordered_map<ServiceID, bool> __serviceIdExistsMap = {
    {ID_ONLINE_PULL_REQ, true},
    {ID_ONLINE_PULL_RSP, true},
    {ID_PING_PONG_REQ, true},
    {ID_PING_PONG_RSP, true},
    {ID_CHAT_LOGIN_INIT, true},
    {ID_CHAT_LOGIN_INIT_RSP, true},
    {ID_SEARCH_USER_REQ, true},
    {ID_SEARCH_USER_RSP, true},
    {ID_ADD_FRIEND_REQ, true},
    {ID_ADD_FRIEND_RSP, true},
    {ID_NOTIFY_ADD_FRIEND_REQ, true},
    {ID_AUTH_FRIEND_REQ, true},
    {ID_AUTH_FRIEND_RSP, true},
    {ID_NOTIFY_AUTH_FRIEND_REQ, true},
    {ID_PUSH_TEXT_MSG_REQ, true},
    {ID_TEXT_CHAT_MSG_RSP, true},
    {ID_NOTIFY_PUSH_TEXT_MSG_REQ, true},
};

// 查询服务ID是否存在
inline bool __IsSupportServiceID(ServiceID id) {
  return __serviceIdExistsMap.find(id) != __serviceIdExistsMap.end();
}

inline std::string __MapSerivceIdToString(ServiceID id) {
  if (__IsSupportServiceID(id)) {
    return __serviceIdMap[id];
  }
  return "Unknown Service ID";
}

// redis key 前缀
#define PREFIX_REDIS_UIP "uip_"
#define PREFIX_REDIS_USER_TOKEN "utoken_"
#define PREFIX_REDIS_IP_COUNT "ipcount_"
#define PREFIX_REDIS_USER_INFO "ubaseinfo_"
#define PREFIX_REDIS_USER_ACTIVE_COUNT "logincount"
#define PREFIX_REDIS_NAME_INFO "nameinfo_"