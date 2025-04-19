#include "net.h"

namespace IM {
void onReWrite(int seq, std::shared_ptr<tcp::socket> socket,
               const std::string &package, int serviceID, int callcount) {
  base::pushMessage(socket, serviceID, package);

  auto waitAckTimer =
      std::make_shared<net::steady_timer>(socket->get_executor());
  waitAckTimerMap[seq] = waitAckTimer;
  waitAckTimer->expires_after(std::chrono::seconds(10));

  waitAckTimer->async_wait([seq, &waitAckTimer, socket, package,
                            callcount](const boost::system::error_code &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      spdlog::info("ACK timer canceled");
      waitAckTimerMap.erase(seq);
      return;
    } else if (ec == boost::asio::error::timed_out) {
      static const int max_timeout_count = 3;
      if (callcount + 1 > max_timeout_count) {
        socket->close();
        return;
      }
      waitAckTimerMap.erase(seq);
      waitAckTimer.reset();
      onReWrite(seq, socket, package, callcount + 1);
    }
  });
}

void readRun(std::shared_ptr<tcp::socket> socket, char *buf) {
  socket->async_receive(
      net::buffer(buf, strlen(buf)),
      [=](const boost::system::error_code &ec, std::size_t) {
        if (ec) {
          spdlog::error("receive message failed, error is {}", ec.message());
          return;
        }
        int id = 0, total = 0;
        memcpy(&id, buf, sizeof(id));
        memcpy(&total, buf + sizeof(id), sizeof(total));
        id = net::detail::socket_ops::network_to_host_long(id);
        total = net::detail::socket_ops::network_to_host_long(total);
        std::string data(total, '\0');
        memcpy(data.data(), buf + sizeof(id) + sizeof(total), total);
        std::shared_ptr<TLV> tlv = std::make_shared<TLV>(id, total, data);
        handleRun(socket, tlv);

        memset(buf, 0, strlen(buf));
        readRun(socket, buf);
      });
}

static std::set<int> seqCache;
static std::map<int, bool> seqCacheExpireMap;

void seqCacheExpire(int seq) { seqCacheExpireMap[seq] = true; }
void seqCacheCancel(int seq) { seqCacheExpireMap[seq] = false; }

void globalTimerCacheCheck(std::shared_ptr<tcp::socket> socket) {
  std::shared_ptr<boost::asio::steady_timer> timer(
      new boost::asio::steady_timer(socket->get_executor()));
  timer->expires_after(std::chrono::seconds(10));
  timer->async_wait([&timer, &socket](const boost::system::error_code &ec) {
    if (ec == boost::asio::error::timed_out) {
      for (auto &seq : seqCache) {
        bool isUpdate = seqCacheExpireMap[seq];
        if (!isUpdate) {
          seqCache.erase(seq);
          seqCacheExpireMap.erase(seq);
        } else {
          seqCacheCancel(seq);
        }
      }

      timer.reset();
      globalTimerCacheCheck(socket);
    }
  });
}

void Text_recv_rsp_handler(int from, int to, int seq, std::string text) {
  spdlog::info("[{}]: {}\t|seq:{}", from, text, seq);
}
void handleRun(std::shared_ptr<tcp::socket> socket, std::shared_ptr<TLV> tlv) {
  static const int ID_TEXT_RECV_RSP = 1021;
  static const int ID_TEXT_SEND_ACK = 1022;

  Json::Value recvRsp;
  Json::Reader reader;
  reader.parse(tlv->data, recvRsp);
  auto errcode = recvRsp["error"].asInt();
  if (errcode == -1) {
    assert(0);
  }

  if (tlv->id == ID_TEXT_RECV_RSP) {
    int from = recvRsp["fromUid"].asInt();
    int to = recvRsp["toUid"].asInt();
    std::string text = recvRsp["text"].asString();
    int seq = recvRsp["seq"].asInt();
    bool missCache = seqCache.find(seq) != seqCache.end();
    Json::Value ack;
    ack["seq"] = seq;
    ack["fromUid"] = from;
    ack["toUid"] = to;
    bool asyncFlag = true;
    // 若ACK已被服务端收到，则意味着其不会重发，反之则重发，若其重发，则复发一次ACK给客户端
    if (!missCache) {
      Text_recv_rsp_handler(from, to, seq, text);
      seqCache.insert(seq);
      seqCacheExpireMap[seq] = false;
      base::pushMessage(socket, ID_TEXT_SEND_ACK, ack.toStyledString(),
                        asyncFlag);
    } else {
      base::pushMessage(socket, ID_TEXT_SEND_ACK, ack.toStyledString(),
                        asyncFlag);
      seqCacheExpire(seq);
    }
  } else if (tlv->id == ID_TEXT_SEND_RSP) {
    // 得到的是该客户端请求前自己分配的seq，并非服务端seq
    int seq = recvRsp["seq"].asInt();
    waitAckTimerMap[seq]->cancel();
    spdlog::info("ACK seq:{}", seq);
  }
}

std::shared_ptr<tcp::socket>
reConnect(const std::string &ip, unsigned short port, net::io_context &ioc) {
  auto newSocket = base::startChatClient(ioc, ip, port);
  return newSocket;
}

void arrhythmiaHandle(std::shared_ptr<tcp::socket> &gateSocket,
                      std::shared_ptr<tcp::socket> &chatSocket,
                      net::io_context &ioc, int uid) {
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
      *gateSocket, req,
      [=, &chatSocket, &gateSocket, &ioc](const boost::system::error_code &ec,
                                          std::size_t) {
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
            [=, &chatSocket, &ioc](beast::error_code ec,
                                   std::size_t bytes_transferred) {
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
                auto ip = tmp["ip"].asString();
                auto port = tmp["port"].asInt();

                chatSocket = reConnect(ip, port, ioc);
                spdlog::info("reconnect chat server success | uid-{}", uid);
              }
            });
      });
}

// 暂行，若想不写该函数而复用net.h中的onReWrite函数，则需要设定处理函数、
// 以及int -> timer中保证唯一性（考虑用前缀值使uid、seq的映射唯一）
void onRePingWrite(int uid, std::shared_ptr<tcp::socket> &gateSocket,
                   std::shared_ptr<tcp::socket> &chatSocket,
                   net::io_context &ioc, int count) {
  Json::Value ping;
  ping["uid"] = uid;

  chatSocket->async_send(
      net::buffer(ping.toStyledString()),
      [=, &uid, &gateSocket, &chatSocket,
       &ioc](const boost::system::error_code &ec, std::size_t) {
        if (ec) {
          spdlog::error("send heartbeat failed, error is {}", ec.message());
          return;
        }
        std::shared_ptr<net::steady_timer> timer(
            new net::steady_timer(chatSocket->get_executor()));
        waitPongTimerMap[uid] = timer;
        timer->expires_after(std::chrono::seconds(5));
        timer->async_wait([=, &count, &uid, &gateSocket, &chatSocket,
                           &ioc](boost::system::error_code ec) {
          if (ec == boost::asio::error::operation_aborted) {
            spdlog::info("onRePongWrite timer canceled | uid-{}", uid);
            waitPongTimerMap.erase(uid);
            return;
          } else if (ec == boost::asio::error::timed_out) {
            // 暂行方案，可进一步考虑租约机制，未超时采用n秒租约以免于频繁Ping，若超时一次后则采用默认心跳
            static const int max_retry_count = 3;
            if (count + 1 > max_retry_count) {
              arrhythmiaHandle(gateSocket, chatSocket, ioc, uid);
              return;
            }
            onRePingWrite(uid, gateSocket, chatSocket, ioc, count + 1);
            return;
          }
        });
      });
}
}; // namespace IM