#pragma once

#include "ChatSession.h"

#include <boost/asio.hpp>
#include <memory.h>
#include <mutex>
#include <unordered_map>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net {
using namespace boost::asio;
using boost::system::error_code;
} // namespace net

namespace wim {

// ChatServer类用于受理连接和管理连接资源
// Start()在受理新连接的同时将开启该连接的会话
class ChatServer {
public:
  ChatServer() = delete;
  ChatServer(net::io_context &ioContext, uint16_t port);
  ~ChatServer();

  uint64_t GetSessionID();

  void ClearSession(uint64_t id);
  void Start();

private:
  void HandleAccept(ChatSession::Ptr, const net::error_code &error);

private:
  net::io_context &acceptContext;
  uint16_t Port;
  tcp::acceptor Acceptor;
  std::mutex Mutex;
  std::atomic<uint64_t> sessionID;
  std::unordered_map<uint64_t, ChatSession::Ptr> sessionGroup;
};

}; // namespace wim