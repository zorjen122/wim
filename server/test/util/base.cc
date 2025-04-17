#include "base.h"
#include "json/reader.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cassert>
#include <cstring>
#include <string>
#include <unordered_map>

namespace base {
UserManager userManager{};

Json::Value buildRegPackage() {
  Json::Value pack;

  auto email = generateRandomEmail();
  auto name = email.substr(0, email.find('@'));

  pack["email"] = email;
  pack["name"] = name;
  pack["password"] = PASSWORD;
  pack["confirm"] = PASSWORD;
  pack["icon"] = "null";

  return pack;
}

std::shared_ptr<net::ip::tcp::socket>
startChatClient(net::io_context &io_context, const std::string &host,
                const std::string &port) {
  boost::system::error_code ec;

  std::string server_address = host;               // 服务器地址
  unsigned short server_port = atoi(port.c_str()); // 服务器端口

  // 创建 TCP 协议的 socket
  std::shared_ptr<net::ip::tcp::socket> socket(
      new net::ip::tcp::socket(io_context));

  // 解析目标地址
  net::ip::tcp::endpoint endpoint(net::ip::make_address(server_address),
                                  server_port);

  spdlog::info("[connect...]");
  // 连接到服务器
  socket->connect(endpoint, ec);
  if (ec.failed()) {
    spdlog::error("connect is wrong!");
    return nullptr;
  }

  spdlog::info("connect im-server is success |  {} : {} ", host, port);
  return socket;
}

void pushMessage(std::shared_ptr<net::ip::tcp::socket> socket,
                 unsigned int serviceID, const std::string &package,
                 bool async) {
  try {
    Json::Value data;
    serviceID = net::detail::socket_ops::host_to_network_long(serviceID);
    unsigned int packageSize = net::detail::socket_ops::host_to_network_long(
        package.size()); // 使用 host_to_network_long 转换为网络字节序的 4
                         // 字节整数

    char servicePackage[4096]{}; // 2 字节 serviceID + 4 字节 packageSize = 6
                                 // 字节

    memcpy(servicePackage, &serviceID,
           sizeof(serviceID)); // 将 serviceID 的 2 字节复制到 servicePackage
    memcpy(servicePackage + sizeof(serviceID), &packageSize,
           4); // 将 packageSize 的 4 字节复制到 servicePackage
    memcpy(servicePackage + sizeof(serviceID) + sizeof(packageSize),
           package.c_str(), package.size());

    spdlog::info("serviceID {}, packageSize {}",
                 *(unsigned int *)(servicePackage), packageSize);
    spdlog::info("source serviceID {}",
                 net::detail::socket_ops::network_to_host_long(
                     *(unsigned int *)(servicePackage)));
    spdlog::info(
        "source packageSize {}",
        net::detail::socket_ops::network_to_host_long(
            *(unsigned int *)(servicePackage +
                              sizeof(
                                  serviceID)))); // 反序列化为原始的 packageSize

    // spdlog::info("[service-package] {}", servicePackage);

    if (async) {
      net::async_write(
          *socket,
          net::buffer(servicePackage,
                      sizeof(serviceID) + sizeof(packageSize) + package.size()),
          [](const boost::system::error_code &ec, std::size_t bytes) {
            if (ec) {
              spdlog::error("async-write is wrong! | ec {} ", ec.message());
            }
          });
    } else {
      net::write(*socket, net::buffer(servicePackage, sizeof(serviceID) +
                                                          sizeof(packageSize) +
                                                          package.size()));
    }
  } catch (const std::exception &e) {
    spdlog::error("Error: {} ", e.what());
  }
} // namespace test

std::string recviceMessage(std::shared_ptr<net::ip::tcp::socket> socket) {
  unsigned int id = 0, total = 0;

  char headBuf[9] = {};

  auto rt = socket->receive(net::buffer(headBuf, sizeof(id) + sizeof(total)));
  headBuf[rt] = 0;

  memcpy(&id, headBuf, sizeof(id));
  memcpy(&total, headBuf + sizeof(id), +sizeof(total));
  id = net::detail::socket_ops::network_to_host_long(id);
  total = net::detail::socket_ops::network_to_host_long(total);
  spdlog::info("recvice message [id: {}, body-total: {}]", id, total);

  std::string bodyBuf(total + 1, '\0');
  rt = socket->receive(net::buffer(bodyBuf.data(), total));
  bodyBuf[total] = 0;

  spdlog::info("recvice message [body : {}]", bodyBuf);

  return bodyBuf;
}

std::string post(const std::string &host, const std::string &port,
                 const std::string &path, const std::string &data,
                 std::unordered_map<std::string, std::string> headers) {

  net::io_context ioc;
  tcp::resolver resolver(ioc);
  boost::system::error_code ec;
  auto const results = resolver.resolve(host, port, ec);
  if (ec) {
    spdlog::error("reslove is wrong! | ec {} ", ec.message());
    return {};
  }

  tcp::socket sock(ioc);
  net::connect(sock, results.begin(), results.end(), ec);
  if (ec) {
    spdlog::error("connect is wrong!");
    return {};
  }

  http::request<http::string_body> req{http::verb::post, path, 11};
  req.set(http::field::host, host);
  req.set(http::field::content_type, "application/json");
  if (headers.size() > 0) {
    for (auto [k, v] : headers) {
      req.set(k, v);
    }
  }
  req.body() = data;
  req.prepare_payload();

  std::clock_t t = std::clock();
  http::write(sock, req, ec);

  if (ec) {
    spdlog::error("post: send as wrong!\n");
    return {};
  }
  // 读取响应
  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  http::read(sock, buffer, res, ec);

  if (ec) {
    spdlog::error("post: read as wrong!\n");
    return {};
  }

  std::clock_t t2 = std::clock();

  double ms = double(t2 - t) / CLOCKS_PER_SEC * 1000;
  auto rsp = boost::beast::buffers_to_string(res.body().data());
  spdlog::info("[time-{}(ms), response: {}]", ms, rsp);

  return rsp;
}
} // namespace base