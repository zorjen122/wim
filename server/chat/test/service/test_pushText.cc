#include "base.h"
#include "spdlog/spdlog.h"
#include "json/reader.h"
#include "json/value.h"
#include <boost/asio/ip/host_name.hpp>
#include <memory>

void sender(std::shared_ptr<net::ip::tcp::socket> socket, int from, int to) {

  Json::Value req1;
  req1["from"] = from;
  req1["to"] = to;
  req1["text"] = "Hello, IM!";
  base::pushMessage(socket, ID_PUSH_TEXT_MESSAGE, req1.toStyledString());
}

auto receiver(std::shared_ptr<net::ip::tcp::socket> socket) {
  return base::recviceMessage(socket);
}

void loginHandle(std::shared_ptr<net::ip::tcp::socket> socket) {

  Json::Value req1;
  req1["uid"] = 2;

  base::pushMessage(socket, 1005, req1.toStyledString());
  spdlog::info("total message: {}", req1.toStyledString());
  base::recviceMessage(socket);
}

int main() {

  fetchUsersFromDatabase(&base::userManager);

  net::io_context ioc;
  std::shared_ptr<net::ip::tcp::socket> socket{};

  auto &user = base::userManager.front();
  spdlog::info("[fetch-user]: id {}, email {}, password {} ", user.id,
               user.email, user.password);

  // base::login(user.uid);

  int from = user.uid, to{};
  for (;;) {
    to = generateRandomNumber(base::userManager.size());
    if (to != from)
      break;
  }

  // toNormalString(user.host);
  // toNormalString(user.port);
  // assert(!user.host.empty());
  // assert(!user.port.empty());
  user.host = "127.0.0.1";
  user.port = "8090";
  spdlog::info("[login-user]: id {}, host {}, port {} ", user.id, user.host,
               user.port);

  socket = base::startChatClient(ioc, user.host, user.port);

  loginHandle(socket);
  sender(socket, from, to);
  std::string rt = receiver(socket);

  spdlog::info("Send ACK....");
  if (!rt.empty()) {
    constexpr int ID_ACK = 0xff33;
    base::pushMessage(socket, ID_ACK, rt);
    rt = receiver(socket);
    if (!rt.empty()) {
      spdlog::info("[Send message is success!]");
    } else {
      spdlog::info("[Send-message is failed!]");
    }
  } else {
    spdlog::info("[Recvice message is failed!]");
  }

  ioc.run();
  socket->close();
  ioc.stop();

  return 0;
}
