#include "base.h"
#include "util.h"
#include <boost/asio/io_context.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unistd.h>

int loginHandle(std::shared_ptr<net::ip::tcp::socket> socket, int uid) {

  Json::Value req1;
  req1["uid"] = uid;

  base::pushMessage(socket, 1005, req1.toStyledString());
  spdlog::info("total message: {}", req1.toStyledString());
  auto rt = base::recviceMessage(socket);

  return rt.empty() ? -1 : 0;
}

int ack(std::shared_ptr<net::ip::tcp::socket> socket, std::string rt) {
  spdlog::info("Send ACK....");
  if (!rt.empty()) {
    constexpr int ID_ACK = 0xff33;
    base::pushMessage(socket, ID_ACK, rt);
    rt = base::recviceMessage(socket);
    if (!rt.empty()) {
      spdlog::info("[ack is success!]");
    } else {
      spdlog::info("[ack is failed!]");
      return -1;
    }
  } else {
    spdlog::info("[Recvice message is failed!]");
    return -1;
  }

  return 0;
}

int groupRecv(std::shared_ptr<net::ip::tcp::socket> socket) {

  spdlog::info("receive...");
  auto rt = base::recviceMessage(socket);

  sleep(2);

  spdlog::info("send ACK....");
  return ack(socket, rt);
}

int groupJoin(std::shared_ptr<net::ip::tcp::socket> socket, int from, int gid) {
  Json::Value req1;
  req1["from"] = from;
  req1["gid"] = gid;

  base::pushMessage(socket, 1025, req1.toStyledString());
  auto rt = base::recviceMessage(socket);
  return rt.empty() ? -1 : 0;
}

int groupCreate(std::shared_ptr<net::ip::tcp::socket> socket, int up) {
  Json::Value req1;
  req1["uid"] = up;
  // req1["gid"] = generateRandomNumber(10000000, 999999999);
  req1["gid"] = 1001;

  base::pushMessage(socket, 1023, req1.toStyledString());
  auto rt = base::recviceMessage(socket);
  return rt.empty() ? -1 : 0;
};

int test(int argc, char *argv[]) {

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

  int q{};
  q = loginHandle(socket, user.uid);
  if (q == 0)
    spdlog::info("login success");
  else
    spdlog::error("login failed");

  std::string rt{};
  if (argc > 2 && strcmp(argv[2], "create-group") == 0) {
    q = groupCreate(socket, user.uid);
    if (q == 0)
      spdlog::info("group create success");
    else
      spdlog::error("group create failed");
  }

  q = groupJoin(socket, user.uid, 1001);
  if (q == 0)
    spdlog::info("group join success");
  else
    spdlog::error("group join failed");

  q = groupRecv(socket);
  if (q == 0)
    spdlog::info("group recv success");
  else
    spdlog::error("group recv failed");

  ioc.run();

  return 0;
}