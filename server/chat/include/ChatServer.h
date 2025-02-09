#pragma once
#include <memory.h>

#include <boost/asio.hpp>
#include <map>
#include <mutex>

#include "ChatSession.h"

#include <memory>
#include <mutex>
#include <unordered_map>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net {
using namespace boost::asio;
using boost::system::error_code;
} // namespace net

class ChatServer {
public:
  ChatServer() = delete;
  ChatServer(net::io_context &ioContext, unsigned short port);
  ~ChatServer();

  size_t GetSessionID();

  void ClearSession(size_t id);
  void Start();

private:
  void HandleAccept(std::shared_ptr<ChatSession>, const net::error_code &error);

private:
  net::io_context &Ioc;
  unsigned short Port;
  tcp::acceptor Acceptor;
  std::mutex Mutex;
  std::atomic<size_t> count;
  std::unordered_map<size_t, std::shared_ptr<ChatSession>> sessionGroup;
};