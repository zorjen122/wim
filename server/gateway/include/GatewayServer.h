#pragma once

#include <boost/asio.hpp>

namespace wim::connection {

class MessageLinkManager;
class SessionRegistry;

class GatewayServer {
 public:
  GatewayServer(boost::asio::io_context &ioContext, unsigned short port,
                SessionRegistry &registry, MessageLinkManager &messageLinks,
                boost::asio::thread_pool &businessPool);

  boost::asio::awaitable<void> Run();

 private:
  boost::asio::io_context &ioContext;
  boost::asio::ip::tcp::acceptor acceptor;
  SessionRegistry &registry;
  MessageLinkManager &messageLinks;
  boost::asio::thread_pool &businessPool;
};

}  // namespace wim::connection
