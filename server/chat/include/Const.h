#pragma once
#include <functional>

enum ErrorCodes {
  Success = 0,
  JsonParserErr = 1001,  // Json????????
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
  UserNotOnline,
  UserNotFriend,
  UserOnline,
  UserOffline
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
#include <limits.h>
#define PACKAGE_MAX_LENGTH (UINT_MAX)
#define PACKAGE_TOTAL_LEN 8
#define PACKAGE_ID_LEN 4
#define PACKAGE_DATA_SIZE_LEN 4
#define MAX_RECV_QUEUE_LEN 10000
#define MAX_SEND_QUEUE_LEN 1000

#include <string>
#include <unordered_map>

enum ServiceID {
  ID_ONLINE_PULL_REQ = 1001, // 在线拉取
  ID_ONLINE_PULL_RSP = 1002,
  ID_PING_PONG_REQ = 1003, // 心跳
  ID_PING_PONG_RSP = 1004,
  ID_CHAT_LOGIN_INIT = 1005, // 登录
  ID_CHAT_LOGIN_INIT_RSP = 1006,
  ID_SEARCH_USER_REQ = 1007, // 搜索
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
  ID_TEXT_SEND_REQ = 1020, // 发送消息
  ID_TEXT_SEND_RSP = 1021,

  ID_GROUP_CREATE_REQ = 1023,
  ID_GROUP_CREATE_RSP = 1024,
  ID_GROUP_JOIN_REQ = 1025,
  ID_GROUP_JOIN_RSP = 1026,
  ID_GROUP_TEXT_SEND_REQ = 1027,
  ID_GROUP_TEXT_SEND_RSP = 1028,
  ID_REMOVE_FRIEND_REQ = 1030,
  ID_REMOVE_FRIEND_RSP = 1031,
  ID_USER_QUIT_WAIT_REQ = 1032,
  ID_USER_QUIT_WAIT_RSP = 1033,

  ID_USER_QUIT_GROUP_REQ = 1034,
  ID_USER_QUIT_GROUP_RSP = 1035,

  ID_LOGIN_SQUEEZE = 0xff01,

  // about service handles of the utility
  ID_UTIL_ACK_SEQ = 0xff33,
  ID_UTIL_ACK_RSP = 0xff34,
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
    {ID_UTIL_ACK_SEQ, "Util Ack Sequence"},
    {ID_UTIL_ACK_RSP, "Util Ack Response"},

    // dev..
    {ID_GROUP_CREATE_REQ, "Group Create Request"},
    {ID_GROUP_CREATE_RSP, "Group Create Response"},
    {ID_GROUP_JOIN_REQ, "Group Join Request"},
    {ID_GROUP_JOIN_RSP, "Group Join Response"},
    {ID_TEXT_SEND_REQ, "Text Send Request"},
    {ID_TEXT_SEND_RSP, "Text Send Response"},
    {ID_GROUP_TEXT_SEND_REQ, "Group Text Send Request"},
    {ID_GROUP_TEXT_SEND_RSP, "Group Text Send Response"},

    {ID_REMOVE_FRIEND_REQ, "Remove Friend Request"},
    {ID_REMOVE_FRIEND_RSP, "Remove Friend Response"},
    {ID_USER_QUIT_WAIT_REQ, "User Quit Wait Request"},
    {ID_USER_QUIT_WAIT_RSP, "User Quit Wait Response"},
    {ID_USER_QUIT_GROUP_REQ, "User Quit Group Request"},
    {ID_USER_QUIT_GROUP_RSP, "User Quit Group Response"},
    {ID_LOGIN_SQUEEZE, "Login Squeeze"},
};

// 查询服务ID是否存在
inline bool __IsSupportServiceID(ServiceID id) {
  return __serviceIdMap.find(id) != __serviceIdMap.end();
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