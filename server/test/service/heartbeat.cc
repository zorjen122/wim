#include "base.h"
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <json/json.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

std::map<int, std::shared_ptr<net::steady_timer>> waitPongTimerMap;
std::shared_ptr<tcp::socket> gateSocket;
std::shared_ptr<net::ip::tcp::socket> chatSocket{};
net::io_context globalIoc;
std::shared_ptr<tcp::socket> InitGateServer(net::io_context &ioc,
                                            const std::string &host,
                                            const std::string &port) {
  tcp::resolver resolver(ioc);
  boost::system::error_code ec;
  auto const results = resolver.resolve(host, port, ec);
  if (ec) {
    spdlog::error("reslove is wrong! | ec {} ", ec.message());
    return nullptr;
  }

  std::shared_ptr<tcp::socket> socket(new tcp::socket(ioc));
  net::connect(*socket, results.begin(), results.end(), ec);
  if (ec) {
    spdlog::error("connect is wrong!");
    return nullptr;
  }

  return socket;
}

std::shared_ptr<tcp::socket> reConnect(const std::string &host,
                                       const std::string &port,
                                       net::io_context &ioc) {
  auto newSocket = base::startChatClient(ioc, host, port);
  return newSocket;
}
void arrhythmiaHandle(int uid) {
  std::string path = "/post-arrhythmia";
  Json::Value userInfo;
  userInfo["uid"] = uid;
  http::request<http::string_body> req{http::verb::post, path, 11};

  auto host = gateSocket->remote_endpoint().address().to_string();
  req.set(http::field::host, host);
  req.set(http::field::content_type, "application/json");
  req.body() = userInfo.toStyledString();
  req.prepare_payload();

  http::async_write(
      *gateSocket, req, [=](const boost::system::error_code &ec, std::size_t) {
        if (ec) {
          spdlog::error("post: send as wrong!\n");
          return;
        }
        // 读取响应
        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        //考虑同步等待响应，因其已无法使用聊天服务
        http::async_read(
            *gateSocket, buffer, res,
            [=](beast::error_code ec, std::size_t bytes_transferred) {
              if (ec.failed()) {
                spdlog::error("post: read as wrong!\n");
                return;
              }

              if (res.result() == http::status::ok) {
                // get response(no source http header)
                auto rsp = beast::buffers_to_string(res.body().data());
                spdlog::info("response: {}", rsp);
                Json::Reader reader;
                Json::Value tmp;
                reader.parse(rsp, tmp);
                auto errcode = tmp["error"].asInt();
                if (errcode == -1) {
                  spdlog::warn(
                      "arrhythmiaHandle: uid-{} arrhythmia failed about "
                      "state request, error is {}",
                      uid, errcode);
                  return;
                }
                auto host = tmp["host"].asString();
                auto port = tmp["port"].asString();

                chatSocket = reConnect(host, port, globalIoc);
                spdlog::info("reconnect chat server success | uid-{}", uid);
              }
            });
      });
}

// 暂行，若想不写该函数而复用net.h中的onReWrite函数，则需要设定处理函数、
// 以及int -> timer中保证唯一性（考虑用前缀值使uid、seq的映射唯一）
void onRePingWrite(int uid, std::shared_ptr<tcp::socket> socket,
                   int count = 0) {
  Json::Value ping;
  ping["uid"] = uid;

  socket->async_send(
      net::buffer(ping.toStyledString()),
      [=, &uid](const boost::system::error_code &ec, std::size_t) {
        if (ec) {
          spdlog::error("send heartbeat failed, error is {}", ec.message());
          return;
        }
        std::shared_ptr<net::steady_timer> timer(
            new net::steady_timer(socket->get_executor()));
        waitPongTimerMap[uid] = timer;
        timer->expires_after(std::chrono::seconds(5));
        timer->async_wait([=, &count](boost::system::error_code ec) {
          if (ec == boost::asio::error::operation_aborted) {
            spdlog::info("onRePongWrite timer canceled | uid-{}", uid);
            waitPongTimerMap.erase(uid);
            return;
          } else if (ec == boost::asio::error::timed_out) {
            // 暂行方案，可进一步考虑租约机制，未超时采用n秒租约以免于频繁Ping，若超时一次后则采用默认心跳
            static const int max_retry_count = 3;
            if (count + 1 > max_retry_count) {
              arrhythmiaHandle(uid);
              return;
            }
            onRePingWrite(uid, socket, count + 1);
            return;
          }
        });
      });
}

void heartbeatRun(int uid, std::shared_ptr<tcp::socket> socket) {}

int test() {
  fetchUsersFromDatabase(&base::userManager);

  auto &user = base::userManager.back();
  spdlog::info("[fetch-user]: id {}, email {}, password {} ", user.id,
               user.email, user.password);

  user.host = "127.0.0.1";
  user.port = "8090";
  spdlog::info("[login-user]: id {}, host {}, port {} ", user.id, user.host,
               user.port);

  chatSocket = base::startChatClient(globalIoc, user.host, user.port);

  Json::Value loginReq;
  loginReq["uid"] = 20;
  //..
  base::pushMessage(chatSocket, ID_CHAT_LOGIN_REQ, loginReq.toStyledString());
  auto val = base::recviceMessage(chatSocket);
  Json::Value loginRsp;
  Json::Reader reader;
  reader.parse(val, loginRsp);
  auto errcode = loginRsp["error"].asInt();
  if (errcode == -1) {
    assert(0);
  }

  InitGateServer(globalIoc, "127.0.0.1", "8080");
  onRePingWrite(user.id, chatSocket);
  char buf[1024]{};
  chatSocket->async_receive(
      net::buffer(buf, strlen(buf)),
      [=](boost::system::error_code ec, std::size_t bytes_transferred) {
        Json::Value textSendRsp;
        Json::Reader reader;
        auto root = std::string(buf, bytes_transferred);
        reader.parse(root, textSendRsp);

        auto code = textSendRsp["status"].asString();
        if (!code.empty() && code == "ack") {
          auto waitAckTimer = waitPongTimerMap[user.id];
          waitAckTimer->cancel();
        }
      });
  globalIoc.run();
  return 0;
}