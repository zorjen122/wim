#pragma once

#include <json/reader.h>
#include <json/value.h>
#include <memory>
#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/system/detail/error_code.hpp>

#include <boost/random.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <string>

#include <ctime>
#include <string>

#include "util.h"

namespace beast = boost::beast;      // 从 Boost.Beast 中引入别名
namespace http = boost::beast::http; // 使用 HTTP 模块
namespace net = boost::asio;         // 使用 Asio 库的网络模块

#define PASSWORD "rootroot";
#define ID_PUSH_TEXT_MESSAGE 1020
#define ID_ACK_MESSAGE 0xff33

using tcp = net::ip::tcp; // TCP/IP 协议

namespace base {
extern UserManager userManager;

Json::Value buildRegPackage();
std::shared_ptr<net::ip::tcp::socket>
startChatClient(net::io_context &io_context, const std::string &host,
                const std::string &port);

void pushMessage(std::shared_ptr<net::ip::tcp::socket> socket,
                 unsigned int serviceID, const std::string &package,
                 bool async = 0);
std::string recviceMessage(std::shared_ptr<net::ip::tcp::socket> socket);

void signIn(int uid);
void signUp(int count);

std::string post(const std::string &host, const std::string &port,
                 const std::string &path, const std::string &data,
                 std::unordered_map<std::string, std::string> headers = {});

}; // namespace base

enum ServiceID {
  ID_ONLINE_PULL_REQ = 1001, // 在线拉取
  ID_ONLINE_PULL_RSP = 1002,
  ID_PING_REQ = 1003, // 心跳
  ID_PING_RSP = 1004,
  ID_CHAT_LOGIN_REQ = 1005, // 登录
  ID_CHAT_LOGIN_INIT_RSP = 1006,
  ID_SEARCH_USER_REQ = 1007, // 搜索
  ID_SEARCH_USER_RSP = 1008,
  ID_ADD_FRIEND_REQ = 1009,
  ID_ADD_FRIEND_RSP = 1010,
  ID_NOTIFY_ADD_FRIEND_REQ = 1011,
  ID_AUTH_FRIEND_REQ = 1013,
  ID_AUTH_FRIEND_RSP = 1014,
  ID_NOTIFY_AUTH_FRIEND_REQ = 1015,
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
