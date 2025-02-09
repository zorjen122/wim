#include "base.h"
#include "spdlog/spdlog.h"
#include "util.h"
#include "json/reader.h"
#include "json/value.h"
#include <boost/asio/ip/host_name.hpp>
#include <memory>
#include <string>
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

int loginHandle(std::shared_ptr<net::ip::tcp::socket> socket, int uid) {

  Json::Value req1;
  req1["uid"] = uid;

  base::pushMessage(socket, 1005, req1.toStyledString());
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

int groupJoin(std::shared_ptr<net::ip::tcp::socket> socket, int from, int gid) {
  Json::Value req1;
  req1["from"] = from;
  req1["gid"] = gid;

  base::pushMessage(socket, 1025, req1.toStyledString());
  auto rt = base::recviceMessage(socket);

  return rt.empty() ? -1 : 0;
}

int ack(std::shared_ptr<net::ip::tcp::socket> socket, std::string rt) {
  spdlog::info("Send ACK....");
  if (!rt.empty()) {
    constexpr int ID_ACK = 0xff33;
    base::pushMessage(socket, ID_ACK, rt);
    rt = receiver(socket);
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

int groupMemberText(std::shared_ptr<net::ip::tcp::socket> socket, int gid,
                    int from, int to, std::string text = "Hello, Group!") {
  Json::Value req1;
  req1["gid"] = gid;
  req1["from"] = from;
  req1["to"] = to;
  req1["text"] = text;

  base::pushMessage(socket, 1027, req1.toStyledString());
  auto rt = base::recviceMessage(socket);

  sleep(2);
  spdlog::info("Send ACK....");

  return ack(socket, rt);
}

void testTextSend(User user, std::shared_ptr<net::ip::tcp::socket> socket,
                  int from, int to) {
  sender(socket, from, to);

  sleep(8);

  auto res = receiver(socket);

  if (ack(socket, res) == -1) {
    spdlog::error("ack-failed");
  } else {
    spdlog::info("ack-success");
  }
}

int testGroupMessage(std::shared_ptr<net::ip::tcp::socket> socket, User user,
                     int to) {
  if (loginHandle(socket, user.uid) == -1) {
    spdlog::error("login-failed");
    return -1;
  } else {
    spdlog::info("login-success");
  }

  if (groupJoin(socket, user.uid, 1001) == -1) {
    spdlog::error("group-join-failed");
    return -1;
  } else {
    spdlog::info("group-join-success");
  }

  if (groupMemberText(socket, 1001, user.uid, to, "Hello, Group!") == -1) {
    spdlog::error("group-member-text-failed");
    return -1;
  } else {
    spdlog::info("group-member-text-success");
  }
  return 0;
}

void testPing(std::shared_ptr<net::ip::tcp::socket> socket, User user) {
  int rt;
  rt = loginHandle(socket, user.uid);
  if (rt == -1) {
    spdlog::error("login-failed");
  }
  Json::Value req1;
  req1["uid"] = user.uid;
  sleep(10);
  std::string s;
  s = base::recviceMessage(socket);
  if (s.empty())
    spdlog::error("recvice-message-failed");
  s = base::recviceMessage(socket);
  sleep(10);
}

void testReLogin(std::shared_ptr<net::ip::tcp::socket> socket, User user) {
  int rt;
  rt = loginHandle(socket, user.uid);
  if (rt == -1) {
    spdlog::error("login-failed");
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
  // testGroupMessage(socket, user, to);
  testPing(socket, user);

  ioc.run();

  return 0;
}
