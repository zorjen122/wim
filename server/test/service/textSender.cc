#include "json/reader.h"
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cassert>
#include <json/json.h>
#include <spdlog/spdlog.h>

#include "base.h"
#include "net.h"
#include <boost/asio.hpp>
#include <memory>

net::io_context ioc;
static std::shared_ptr<net::ip::tcp::socket> globalSocket;
bool login() {
  spdlog::info("[fetch-user]: id {}, email {}, password {} ", user.id,
               user.email, user.password);

  user.host = "127.0.0.1";
  user.port = "8090";
  spdlog::info("[login-user]: id {}, host {}, port {} ", user.id, user.host,
               user.port);

  globalSocket = base::startChatClient(ioc, user.host, user.port);

  Json::Value loginReq;
  loginReq["uid"] = 10;
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

  static std::atomic<int> seq{0};

  Json::Value textSendRequest;
  textSendRequest["fromUid"] = 10;
  textSendRequest["toUid"] = 20;
  textSendRequest["text"] = "hello world";

  // 该id仅是客户端的序列号，其作用是在之后接收到服务器ACK能找到并取消重传定时器
  textSendRequest["id"] = seq.load();
  seq += 1;
  onReWrite(seq, globalSocket, textSendRequest.toStyledString(),
            ID_TEXT_SEND_REQ);

  char buf[1024]{};
  readRun(globalSocket, buf);

  ioc.run();
}