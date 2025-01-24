#include "base.h"
#include "util.h"
#include <boost/asio/io_context.hpp>
#include <spdlog/spdlog.h>

void loginHandle(std::shared_ptr<net::ip::tcp::socket> socket, int uid) {

  Json::Value req1;
  req1["uid"] = uid;

  base::pushMessage(socket, 1005, req1.toStyledString());
  spdlog::info("total message: {}", req1.toStyledString());
  base::recviceMessage(socket);
}

int main() {

  using base::userManager;
  fetchUsersFromDatabase(&userManager);

  net::io_context ioc;

  auto user = userManager.front();
  // base::login(user.uid);
  user.host = "127.0.0.1";
  user.port = "8090";

  auto socket = base::startChatClient(ioc, user.host, user.port);

  spdlog::info("receive...");
  loginHandle(socket, user.uid);
  base::recviceMessage(socket);
}