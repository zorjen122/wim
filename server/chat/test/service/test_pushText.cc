#include "base.h"
#include "spdlog/spdlog.h"
#include "util.h"
#include "json/reader.h"
#include "json/value.h"
#include <boost/asio/ip/host_name.hpp>
#include <memory>
#include <unistd.h>

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

void loginHandle(std::shared_ptr<net::ip::tcp::socket> socket, int uid) {

  Json::Value req1;
  req1["uid"] = uid;

  base::pushMessage(socket, 1005, req1.toStyledString());
  base::recviceMessage(socket);
}

void groupCreate(std::shared_ptr<net::ip::tcp::socket> socket, int up) {
  Json::Value req1;
  req1["uid"] = up;
  // req1["gid"] = generateRandomNumber(10000000, 999999999);
  req1["gid"] = 1001;

  base::pushMessage(socket, 1023, req1.toStyledString());
};

void groupJoin(std::shared_ptr<net::ip::tcp::socket> socket, int from,
               int gid) {
  Json::Value req1;
  req1["from"] = from;
  req1["gid"] = gid;

  base::pushMessage(socket, 1025, req1.toStyledString());
}

void groupMemberText(std::shared_ptr<net::ip::tcp::socket> socket, int gid,
                     int from, int to, std::string text = "Hello, Group!") {
  Json::Value req1;
  req1["gid"] = gid;
  req1["from"] = from;
  req1["to"] = to;
  req1["text"] = text;

  base::pushMessage(socket, 1027, req1.toStyledString());
}

void testTextSend(User user, std::shared_ptr<net::ip::tcp::socket> socket,
                  int from, int to) {
  sender(socket, from, to);

  sleep(8);

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
}

int main() {

  fetchUsersFromDatabase(&base::userManager);

  net::io_context ioc;
  std::shared_ptr<net::ip::tcp::socket> socket{};

  auto &user = base::userManager.back();
  spdlog::info("[fetch-user]: id {}, email {}, password {} ", user.id,
               user.email, user.password);

  // base::login(user.uid);

  int from = user.uid, to{};
  for (;;) {
    to = generateRandomNumber(0, base::userManager.size());
    if (to != from)
      break;
  }
  to = base::userManager.front().uid;

  // toNormalString(user.host);
  // toNormalString(user.port);
  // assert(!user.host.empty());
  // assert(!user.port.empty());
  user.host = "127.0.0.1";
  user.port = "8090";
  spdlog::info("[login-user]: id {}, host {}, port {} ", user.id, user.host,
               user.port);

  socket = base::startChatClient(ioc, user.host, user.port);
  loginHandle(socket, user.uid);
  // testTextSend(user, socket, from, to);

  // groupCreate(socket, user.uid);
  // base::recviceMessage(socket);
  // spdlog::info("group-create-success");

  groupJoin(socket, user.uid, 1001);
  base::recviceMessage(socket);

  groupMemberText(socket, 1001, user.uid, to, "Hello, Group!");
  auto rsp = base::recviceMessage(socket);
  Json::Reader reader;
  Json::Value ret;
  bool parserSuccess = reader.parse(rsp, ret);
  if (!parserSuccess) {
    spdlog::info("parse-response-failed");
    return 0;
  }
  if (ret["error"].asInt() != -1) {
    spdlog::info("group-member-text-success, ec: {}", ret["error"].asInt());
  } else {
    spdlog::error("group-member-text-failed, ec: {}", ret["error"].asInt());
  }

  ioc.run();
  socket->close();
  ioc.stop();

  return 0;
}
