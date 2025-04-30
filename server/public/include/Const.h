#pragma once
#include <functional>

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
  Defer(std::function<void()> func) : func(func) {}

  ~Defer() { func(); }

private:
  std::function<void()> func;
};
#include <limits.h>

/* 1.4KB+的缓冲区，指单次读取的最大长度，实际上仅需大于IPv4/IPv6的最大消息单元（MTU）即可
  Pv4 MTU：通常 1500 字节（以太网标准）
  IPv4 最小重组缓冲区：576 字节（RFC 791）
  IPv6 MTU：1280 字节（RFC 8200）
*/

#define PROTOCOL_DATA_MTU 1500
#define PROTOCOL_HEADER_TOTAL 8
#define PROTOCOL_ID_LEN 4
#define PROTOCOL_DATA_SIZE_LEN 4

#define PROTOCOL_RECV_MSS (10 * 1024 * 1024) // 10MB
#define PROTOCOL_SEND_MSS (10 * 1024 * 1024) // 10MB

#include <string>
#include <unordered_map>

enum ErrorCodes {
  Success = 0,
  JsonParser = 1000,
  RPCFailed,
  VarifyExpired,
  VarifyCodeErr,
  UserExist,
  PasswdErr,
  EmailNotMatch,
  PasswdUpFailed,
  PasswdInvalid,
  TokenInvalid,
  UidInvalid,
  UserNotOnline,
  UserNotFriend,
  UserOnline,
  UserOffline,
  NotFound,
  RepeatMessage
};

enum ServiceID {

  /* 拉取 */
  ID_PULL_FRIEND_LIST_REQ = 1001, // 拉取好友列表
  ID_PULL_FRIEND_LIST_RSP,

  ID_PULL_FRIEND_APPLY_LIST_REQ, // 拉取好友申请列表
  ID_PULL_FRIEND_APPLY_LIST_RSP,

  ID_PULL_MESSAGE_LIST_REQ, // 拉取消息列表
  ID_PULL_MESSAGE_LIST_RSP,

  /* 状态 */
  ID_PING_REQ, // 心跳
  ID_PING_RSP,

  ID_LOGIN_INIT_REQ, // 登录
  ID_LOGIN_INIT_RSP,

  ID_USER_QUIT_REQ, // 登出
  ID_USER_QUIT_RSP,

  /* 好友 */
  ID_SEARCH_USER_REQ, // 查找用户
  ID_SEARCH_USER_RSP,

  ID_NOTIFY_ADD_FRIEND_REQ, // 发起好友请求
  ID_NOTIFY_ADD_FRIEND_RSP,

  ID_REPLY_ADD_FRIEND_REQ, // 回应好友请求
  ID_REPLY_ADD_FRIEND_RSP,

  ID_REMOVE_FRIEND_REQ, // 删除好友
  ID_REMOVE_FRIEND_RSP,

  /* 消息 */
  ID_TEXT_SEND_REQ, // 发送文本消息
  ID_TEXT_SEND_RSP,

  ID_ACK, // 确认消息

  /* 群组 */
  ID_GROUP_CREATE_REQ, // 创建群组
  ID_GROUP_CREATE_RSP,

  ID_GROUP_JOIN_REQ, // 加入群组
  ID_GROUP_JOIN_RSP,

  ID_USER_QUIT_GROUP_REQ, // 退出群组
  ID_USER_QUIT_GROUP_RSP,

  ID_GROUP_TEXT_SEND_REQ, // 发送群组消息
  ID_GROUP_TEXT_SEND_RSP,

};

inline int __getServiceResponseId(ServiceID id) { return id + 1; }

// redis key 前缀
#define PREFIX_REDIS_UIP "uip_"
#define PREFIX_REDIS_USER_TOKEN "utoken_"
#define PREFIX_REDIS_IP_COUNT "ipcount_"
#define PREFIX_REDIS_USER_INFO "ubaseinfo_"
#define PREFIX_REDIS_USER_ACTIVE_COUNT "logincount"
#define PREFIX_REDIS_NAME_INFO "nameinfo_"

inline std::string getCurrentDateTime() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);

  static char buffer[80];
  std::tm *now_tm = std::localtime(&now_c);
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);
  std::string dateTime(buffer);
  return dateTime;
}