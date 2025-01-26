#include "base.h"
#include "util.h"
#include <boost/asio/io_context.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

void loginHandle(std::shared_ptr<net::ip::tcp::socket> socket, int uid) {

  Json::Value req1;
  req1["uid"] = uid;

  base::pushMessage(socket, 1005, req1.toStyledString());
  spdlog::info("total message: {}", req1.toStyledString());
  base::recviceMessage(socket);
}

void TextSend(std::shared_ptr<net::ip::tcp::socket> socket) {

  spdlog::info("receive...");
  base::recviceMessage(socket);
}

void groupJoin(std::shared_ptr<net::ip::tcp::socket> socket, int from,
               int gid) {
  Json::Value req1;
  req1["from"] = from;
  req1["gid"] = gid;

  base::pushMessage(socket, 1025, req1.toStyledString());
}

void groupCreate(std::shared_ptr<net::ip::tcp::socket> socket, int up) {
  Json::Value req1;
  req1["uid"] = up;
  // req1["gid"] = generateRandomNumber(10000000, 999999999);
  req1["gid"] = 1001;

  base::pushMessage(socket, 1023, req1.toStyledString());
};

int main(int argc, char *argv[]) {

  using base::userManager;
  fetchUsersFromDatabase(&userManager);

  net::io_context ioc;

  // auto user = userManager.front();

  auto user = userManager[atoi(argv[1])];

  // base::login(user.uid);
  user.host = "127.0.0.1";
  user.port = "8090";

  spdlog::info("[fetch-user]: id {}, email {}, password {} ", user.id,
               user.email, user.password);

  auto socket = base::startChatClient(ioc, user.host, user.port);

  loginHandle(socket, user.uid);

  std::string rt{};
  if (argc > 2 && strcmp(argv[2], "create-group") == 0) {
    groupCreate(socket, user.uid);
    rt = base::recviceMessage(socket);

    Json::Reader reader;
    Json::Value ret;
    bool parserSuccess = reader.parse(rt, ret);
    if (!parserSuccess) {
      spdlog::error("parse json failed");
      return 0;
    }
    if (ret["error"].asInt() == 0) {
      spdlog::info("group create success");
    } else {
      spdlog::error("group create failed");
      return 0;
    }
  }

  groupJoin(socket, user.uid, 1001);
  base::recviceMessage(socket);

  TextSend(socket);

  ioc.run();
  socket->close();
  ioc.stop();

  return 0;
}