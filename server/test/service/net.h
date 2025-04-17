#pragma once
#include "base.h"
#include <boost/asio.hpp>
#include <map>
#include <memory.h>
#include <set>
#include <spdlog/spdlog.h>

namespace IM {
static std::map<int, std::shared_ptr<net::steady_timer>> waitAckTimerMap;

// 注意，目前重发机制仅限于文本消息，可能支持其他消息类型————未验证
static void onReWrite(int seq, std::shared_ptr<tcp::socket> socket,
                      const std::string &package, int serviceID,
                      int callcount = 0) {
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
class TLV {
public:
  int id;
  int total;
  std::string data;

  TLV(int id, int total, std::string data) : id(id), total(total), data(data) {}
};

void handleRun(std::shared_ptr<tcp::socket> socket, std::shared_ptr<TLV> tlv);

static void readRun(std::shared_ptr<tcp::socket> socket, char *buf) {
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
    int from = recvRsp["from"].asInt();
    int to = recvRsp["to"].asInt();
    std::string text = recvRsp["text"].asString();
    int seq = recvRsp["seq"].asInt();
    bool missCache = seqCache.find(seq) != seqCache.end();
    Json::Value ack;
    ack["seq"] = seq;
    ack["from"] = from;
    ack["to"] = to;
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

}; // namespace IM