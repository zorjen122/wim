#include "base.h"
#include "json/value.h"
#include <boost/asio/ip/host_name.hpp>
#include <memory>

int main() {

  fetchUsersFromDatabase(&base::userManager);

  net::io_context ioc;
  std::shared_ptr<net::ip::tcp::socket> socket{};

  // for (auto &user : base::userManager) {
  auto &user = base::userManager.front();
  spdlog::info("[fetch-user]: id {}, email {}, password{} ", user.id,
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

  Json::Value req1;
  req1["from"] = from;
  req1["to"] = to;
  req1["text"] = "Hello, IM!";
  base::pushMessage(socket, ID_PUSH_TEXT_MESSAGE, req1.toStyledString());
  Json::Value rsp(base::recviceMessage(socket));
  spdlog::info("[recvice-message]: {}", rsp.toStyledString());

  // Json::Value req2;
  // req2["seq"] = rsp["seq"].asInt() + 1;
  // base::pushMessage(socket, ID_ACK_MESSAGE, req2.toStyledString());
  //   break;
  // }
  ioc.run();

  socket->close();
  ioc.stop();

  return 0;
}
