#pragma  once

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
#include <iostream>
#include <string>

#include <ctime>
#include <iostream>
#include <string>

#include "util.h"

namespace beast = boost::beast;      // 从 Boost.Beast 中引入别名
namespace http = boost::beast::http; // 使用 HTTP 模块
namespace net = boost::asio;         // 使用 Asio 库的网络模块


#define PASSWORD "rootroot";
#define ID_PUSH_TEXT_MESSAGE 1017

using tcp = net::ip::tcp;            // TCP/IP 协议

namespace base {
  extern UserManager userManager;

  Json::Value buildRegPackage();
  std::shared_ptr<net::ip::tcp::socket> startChatServer(net::io_context& io_context,const std::string &host, const std::string &port);

  void pushService(std::shared_ptr<net::ip::tcp::socket> sock,
                  short serviceID, const std::string &message, int fromID,
                  int toID);
  void login(int uid) ;
  void reg(int count) ;
};