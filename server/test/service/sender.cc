#include "base.h"
#include "spdlog/logger.h"
#include "util.h"
#include <boost/asio/ip/host_name.hpp>
#include <jsoncpp/json/json.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <unistd.h>

inline auto businessLogger = spdlog::stdout_color_mt("test_service_logger");

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
  req1["gid"] = generateRandomNumber(10000000, 999999999);

  base::pushMessage(socket, 1023, req1.toStyledString());
  auto rt = base::recviceMessage(socket);

  return rt.empty() ? -1 : 0;
};

int groupJoin(std::shared_ptr<net::ip::tcp::socket> socket, int from, int gid) {
  Json::Value req1;
  req1["fromUid"] = from;
  req1["gid"] = gid;

  base::pushMessage(socket, 1025, req1.toStyledString());
  auto rt = base::recviceMessage(socket);

  return rt.empty() ? -1 : 0;
}

int ack(std::shared_ptr<net::ip::tcp::socket> socket, std::string rt) {
  businessLogger->info("Send ACK....");
  if (!rt.empty()) {
    constexpr int ID_ACK = 0xff33;
    base::pushMessage(socket, ID_ACK, rt);
    rt = base::recviceMessage(socket);
    if (!rt.empty()) {
      businessLogger->info("[ack is success!]");
    } else {
      businessLogger->info("[ack is failed!]");
      return -1;
    }
  } else {
    businessLogger->info("[Recvice message is failed!]");
    return -1;
  }

  return 0;
}

int sendGroupText(std::shared_ptr<net::ip::tcp::socket> socket, int gid,
                  int from, int to, std::string text = "Hello, Group!") {
  Json::Value req1;
  req1["gid"] = gid;
  req1["fromUid"] = from;
  req1["toUid"] = to;
  req1["text"] = text;

  base::pushMessage(socket, 1027, req1.toStyledString());
  auto rt = base::recviceMessage(socket);

  sleep(2);
  businessLogger->info("Send ACK....");

  return ack(socket, rt);
}

int TextSend(User user, std::shared_ptr<net::ip::tcp::socket> socket, int from,
             int to) {

  Json::Value req1;
  req1["fromUid"] = from;
  req1["toUid"] = to;
  req1["text"] = "Hello, wim!";

  base::pushMessage(socket, ID_PUSH_TEXT_MESSAGE, req1.toStyledString());

  businessLogger->info("Sleep...");
  sleep(8);

  auto res = base::recviceMessage(socket);

  if (ack(socket, res) == -1) {
    businessLogger->error("ack-failed");
    return -1;
  } else {
    businessLogger->info("ack-success");
  }

  return 0;
}

int GroupMessage(std::shared_ptr<net::ip::tcp::socket> socket, User user,
                 int to) {
  if (loginHandle(socket, user.uid) == -1) {
    businessLogger->error("login-failed");
    return -1;
  } else {
    businessLogger->info("login-success");
  }

  if (groupJoin(socket, user.uid, 1001) == -1) {
    businessLogger->error("group-join-failed");
    return -1;
  } else {
    businessLogger->info("group-join-success");
  }

  if (sendGroupText(socket, 1001, user.uid, to, "Hello, Group!") == -1) {
    businessLogger->error("group-member-text-failed");
    return -1;
  } else {
    businessLogger->info("group-member-text-success");
  }
  return 0;
}

// error
int Ping(std::shared_ptr<net::ip::tcp::socket> socket, User user) {
  int rt;
  rt = loginHandle(socket, user.uid);
  if (rt == -1) {
    businessLogger->error("login-failed");
    return -1;
  }

  businessLogger->info("Sleep 10s....");
  sleep(10);

  int cnt{};
  std::string s{};
  while (cnt++ <= 5) {
    businessLogger->info("Wait PING.... | count: {}\n", cnt);

    if (s.empty()) {
      businessLogger->error("recvice-message-failed");
      return -1;
    }
    businessLogger->info("Send PING.... | count: {}\n", cnt);

    businessLogger->info("Sleep 5s....");
    sleep(5);
    base::pushMessage(socket, ID_PING_REQ, s);
  }
  businessLogger->info("[Ping-success!!]");

  return 0;
}

int ReLogin(std::shared_ptr<net::ip::tcp::socket> socket, User user) {
  int rt;
  rt = loginHandle(socket, user.uid);
  if (rt == -1) {
    businessLogger->error("login-failed");

    return -1;
  }

  return 0;
}

auto produceEnvStart() {
  fetchUsersFromDatabase(&base::userManager);

  net::io_context ioc;
  std::shared_ptr<net::ip::tcp::socket> socket{};

  auto &user = base::userManager.back();
  businessLogger->info("[fetch-user]: id {}, email {}, password {} ", user.id,
                       user.email, user.password);

  // base::login(user.uid);

  int from = user.uid, to{};
  for (;;) {
    to = generateRandomNumber(0, base::userManager.size());
    if (to != from)
      break;
  }
  to = base::userManager.front().uid;

  toNormalString(user.host);
  toNormalString(user.port);
  assert(!user.host.empty());
  assert(!user.port.empty());
  user.host = "127.0.0.1";
  user.port = "8090";
  businessLogger->info("[login-user]: id {}, host {}, port {} ", user.id,
                       user.host, user.port);

  socket = base::startChatClient(ioc, user.host, user.port);

  // func()

  ioc.run();
}

int test() {

  fetchUsersFromDatabase(&base::userManager);

  net::io_context ioc;
  std::shared_ptr<net::ip::tcp::socket> socket{};

  auto &user = base::userManager.back();
  businessLogger->info("[fetch-user]: id {}, email {}, password {} ", user.id,
                       user.email, user.password);

  user.host = "127.0.0.1";
  user.port = "8090";
  businessLogger->info("[login-user]: id {}, host {}, port {} ", user.id,
                       user.host, user.port);

  socket = base::startChatClient(ioc, user.host, user.port);

  ioc.run();

  return 0;
}
