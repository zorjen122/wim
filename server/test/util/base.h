#pragma once

#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
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

#include "../../public/include/Const.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

#define PASSWORD "rootroot";
#define ID_PUSH_TEXT_MESSAGE 1020
#define ID_ACK_MESSAGE 0xff33

namespace base {

Json::Value buildRegPackage();
std::shared_ptr<net::ip::tcp::socket>
startChatClient(net::io_context &io_context, const std::string &ip,
                unsigned short port);

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
