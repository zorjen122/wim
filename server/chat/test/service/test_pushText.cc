#include "base.h"
#include <boost/asio/ip/host_name.hpp>
#include <memory>

int main() {

  fetchUsersFromDatabase(&base::userManager);

  net::io_context ioc;
  std::shared_ptr<net::ip::tcp::socket> socket{};

  for (auto &user : base::userManager) {
    spdlog::info("[fetch-user]: id {}, email {}, password{} ", user.id,
                 user.email, user.password);

    base::login(user.uid);

    int from = user.uid, to{};
    for (;;) {
      to = generateRandomNumber(base::userManager.size());
      if (to != from)
        break;
    }

    toNormalString(user.host);
    toNormalString(user.port);

    socket = base::startChatServer(ioc, user.host, user.port);;
    base::pushService( socket,ID_PUSH_TEXT_MESSAGE, std::string("Hello, IM!"), from, to);
    break;
  }

  socket->close();
  ioc.stop();

  return 0;
}
