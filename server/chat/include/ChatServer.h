#pragma once
#include <memory.h>

#include <boost/asio.hpp>
#include <map>
#include <mutex>

#include "ChatSession.h"

using namespace std;
using boost::asio::ip::tcp;

class ChatServer {
 public:
  ChatServer(boost::asio::io_context &io_context, unsigned short port);
  ~ChatServer();
  void ClearSession(const std::string &uuid);
  void Start();

 private:
  void HandleAccept(std::shared_ptr<ChatSession>,
                    const boost::system::error_code &error);

  boost::asio::io_context &_io_context;
  unsigned short _port;
  tcp::acceptor _acceptor;
  std::map<std::string, std::shared_ptr<ChatSession>> _sessions;
  std::mutex _mutex;
};
