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

  // 连接到服务器
  socket->connect(endpoint, ec);
  if (ec) {
    spdlog::error("connect is wrong!");
    return nullptr;
  }

  spdlog::info("connect im-server is success |  {} : {} ", host, port);
  return socket;
}

void pushMessage(std::shared_ptr<net::ip::tcp::socket> socket,
                 unsigned int serviceID, const std::string &package) {
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

    net::write(*socket, net::buffer(servicePackage, sizeof(serviceID) +
                                                        sizeof(packageSize) +
                                                        package.size()));

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

void login(int uid) {

  auto user = userManager[uid];
  auto email = user.email;
  auto password = user.password;

  boost::system::error_code ec;

  net::io_context ioc;

  // 解析主机地址和端口
  tcp::resolver resolver(ioc);
  auto const results = resolver.resolve("localhost", "8080", ec);
  if (ec) {
    spdlog::error("reslove is wrong!");
    return;
  }

  // 建立连接
  tcp::socket sock(ioc);
  net::connect(sock, results.begin(), results.end(), ec);
  if (ec) {
    spdlog::error("connect is wrong!");
    return;
  }

  http::request<http::string_body> req{http::verb::post, "/post_login", 11};
  req.set(http::field::host, "localhost");
  req.set(http::field::content_type, "application/json");

  Json::Value package;
  package["email"] = email;
  package["password"] = password;
  req.body() = package.toStyledString();

  // 对消息体中缺失的必要因素（如content-length）补全
  req.prepare_payload();

  spdlog::info("[login service of the package: {}]", package.toStyledString());
  std::clock_t t = std::clock();
  http::write(sock, req, ec);

  if (ec) {
    spdlog::error("post-login: send as wrong!\n");
    return;
  }
  // 读取响应
  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  http::read(sock, buffer, res, ec);

  if (ec) {
    spdlog::error("post-login: read as wrong!\n");
    return;
  }

  std::clock_t t2 = std::clock();

  double ms = double(t2 - t) / CLOCKS_PER_SEC * 1000;
  auto rsp = boost::beast::buffers_to_string(res.body().data());
  // 输出响应
  spdlog::info("[time-{}(ms), response: {}]", ms, rsp);

  Json::Reader reader;
  Json::Value ret;
  bool parserSuccess = reader.parse(rsp, ret);
  if (!parserSuccess) {
    spdlog::error("[login] parser is wrong");
    return;
  }

  auto host = ret["host"].toStyledString();
  auto port = ret["port"].toStyledString();

  if (host.empty() || port.empty()) {
    spdlog::error("[login] host or port have not such value");
    return;
  }

  userManager[uid].host = host;
  userManager[uid].port = port;

  spdlog::info("login request is success, server [host : {}, port : {}]", host,
               port);

  sock.shutdown(tcp::socket::shutdown_both);
} // namespace base

void reg(int count) {
  try {
    // IO 上下文
    net::io_context ioc;

    // 解析主机地址和端口
    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve("localhost", "8080");

    // 建立连接
    tcp::socket socket(ioc);
    net::connect(socket, results.begin(), results.end());

    // 构建 HTTP POST 请求
    http::request<http::string_body> req{http::verb::post, "/post_register",
                                         11};

    auto data = buildRegPackage();
    std::string package = data.toStyledString();

    req.set(http::field::host, "localhost");
    req.set(http::field::content_type, "application/json");
    req.body() = package; // POST 请求的数据
    req.prepare_payload();

    spdlog::info("[package: {}]", package);

    std::clock_t t = std::clock();
    // 发送 HTTP 请求
    http::write(socket, req);

    // 读取响应
    beast::flat_buffer buffer;
    http::response<http::dynamic_body> res;
    http::read(socket, buffer, res);
    std::clock_t t2 = std::clock();

    double ms = double(t2 - t) / CLOCKS_PER_SEC * 1000;
    auto rsp = boost::beast::buffers_to_string(res.body().data());
    // 输出响应
    spdlog::info("[time-{}(ms), response: {}]", ms, rsp);

    auto user = data["email"].toStyledString();

    auto password = data["password"].toStyledString();

    // 如果上传socket到login，将会发送/post_register，待查
    // 注册后，无法立即查询该注册的条目，待查。
    // login(user, password);

    socket.shutdown(tcp::socket::shutdown_both);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}
}; // namespace base
