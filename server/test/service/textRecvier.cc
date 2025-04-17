#include "json/reader.h"
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <json/json.h>
#include <spdlog/spdlog.h>

#include "base.h"
#include "net.h"
#include <boost/asio.hpp>
#include <memory>
net::io_context ioc;
static std::shared_ptr<net::ip::tcp::socket> globalSocket;
bool login() {
  fetchUsersFromDatabase(&base::userManager);

  auto &user = base::userManager.back();
  spdlog::info("[fetch-user]: id {}, email {}, password {} ", user.id,
               user.email, user.password);

  user.host = "127.0.0.1";
  user.port = "8090";
  spdlog::info("[login-user]: id {}, host {}, port {} ", user.id, user.host,
               user.port);

  globalSocket = base::startChatClient(ioc, user.host, user.port);

  Json::Value loginReq;
  loginReq["uid"] = 20;
  base::pushMessage(globalSocket, ID_CHAT_LOGIN_REQ, loginReq.toStyledString());
  auto val = base::recviceMessage(globalSocket);
  Json::Value loginRsp;
  Json::Reader reader;
  reader.parse(val, loginRsp);
  auto errcode = loginRsp["error"].asInt();
  if (errcode == -1) {
    return false;
  }
  return true;
}
int test() {
  bool loginStatus = login();
  if (!loginStatus) {
    assert(0);
  }
  char buf[1024]{};
  readRun(globalSocket, buf);

  ioc.run();
}