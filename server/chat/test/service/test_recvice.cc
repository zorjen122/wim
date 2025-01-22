#include "base.h"
#include "util.h"
#include <boost/asio/io_context.hpp>
#include <spdlog/spdlog.h>

int main() {

  using base::userManager;
  fetchUsersFromDatabase(&userManager);

  net::io_context ioc;

  auto user = userManager.front();
  base::login(user.uid);

  auto socket = base::startChatClient(ioc, user.host, user.port);

  spdlog::info("receive...");
  base::recviceMessage(socket);
}