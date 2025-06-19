#pragma once

#include "ChatSession.h"

#include <boost/asio.hpp>
#include <memory.h>
#include <mutex>
#include <unordered_map>

namespace wim {

// ChatServer类用于受理连接和管理连接资源
// Start()在受理新连接的同时将开启该连接的会话
class ChatServer {
public:
  using error_code = boost::system::error_code;
  using tcp = boost::asio::ip::tcp;

  ChatServer() = delete;
  ChatServer(io_context &ioContext, uint16_t port);
  ~ChatServer();

  uint64_t GetSessionID();

  void ClearSession(uint64_t id);
  void Start();

private:
  void HandleAccept(ChatSession::ptr, const error_code &error);

private:
  io_context &acceptContext;
  uint16_t Port;
  tcp::acceptor Acceptor;
  std::mutex Mutex;
  std::atomic<uint64_t> sessionID;
  std::unordered_map<uint64_t, ChatSession::ptr> sessionGroup;
};

}; // namespace wim