#pragma once
#include "base.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <map>
#include <memory.h>
#include <set>
#include <spdlog/spdlog.h>

namespace IM {
static std::map<int, std::shared_ptr<net::steady_timer>> waitPongTimerMap;
static std::map<int, std::shared_ptr<net::steady_timer>> waitAckTimerMap;
// 注意，目前重发机制仅限于文本消息，可能支持其他消息类型————未验证
void onReWrite(int seq, std::shared_ptr<tcp::socket> socket,
               const std::string &package, int serviceID, int callcount = 0);
class TLV {
public:
  using Ptr = std::shared_ptr<TLV>;
  int id;
  int total;
  std::string data;

  TLV(int id, int total, std::string data) : id(id), total(total), data(data) {}
};

void handleRun(std::shared_ptr<tcp::socket> socket, std::shared_ptr<TLV> tlv);

void readRun(std::shared_ptr<tcp::socket> socket, char *buf);
void globalTimerCacheCheck(std::shared_ptr<tcp::socket> socket);

void Text_recv_rsp_handler(int from, int to, int seq, std::string text);
void handleRun(std::shared_ptr<tcp::socket> socket, std::shared_ptr<TLV> tlv);

std::shared_ptr<tcp::socket>
reConnect(const std::string &ip, unsigned short port, net::io_context &ioc);

void arrhythmiaHandle(std::shared_ptr<tcp::socket> &gateSocket,
                      std::shared_ptr<tcp::socket> &chatSocket,
                      net::io_context &ioc, int uid);

// 暂行，若想不写该函数而复用net.h中的onReWrite函数，则需要设定处理函数、
// 以及int -> timer中保证唯一性（考虑用前缀值使uid、seq的映射唯一）
void onRePingWrite(int uid, std::shared_ptr<tcp::socket> &gateSocket,
                   std::shared_ptr<tcp::socket> &chatSocket,
                   net::io_context &ioc, int count = 0);
}; // namespace IM