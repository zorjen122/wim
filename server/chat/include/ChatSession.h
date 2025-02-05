#pragma once
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <unistd.h>

#include "Const.h"
#include "Protocol.h"
#include "Timer.h"

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net {
using namespace boost::asio;
using boost::system::error_code;
} // namespace net

class ChatServer;
class Service;

class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
  ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
              size_t id);
  ~ChatSession();

  tcp::socket &GetSocket();
  std::string &GetSessionId();
  net::io_context &GetIoc();

  void Start();
  void Send(char *msgData, unsigned int msgLength, unsigned int msgID);
  void Send(std::string msgData, unsigned int msgID);
  void Close();
  std::shared_ptr<ChatSession> GetSharedSelf();
  void ReceiveBody(size_t size);
  void ReceiveHead(size_t size);
  void ClearSession();

private:
  void asyncReadFull(
      std::size_t maxLength,
      std::function<void(const boost::system::error_code &, std::size_t)>
          handler);
  void
  asyncRead(std::size_t readLen, std::size_t total,
            std::function<void(const boost::system::error_code &, std::size_t)>
                handler);

  void HandleWrite(const boost::system::error_code &error,
                   std::shared_ptr<ChatSession> sharedSelf);

private:
  tcp::socket sock;
  size_t id;
  char packageBuf[PACKAGE_MAX_LENGTH];

  ChatServer *chatServer;
  bool closeEnable;

  std::queue<std::shared_ptr<protocol::SendPackage>> sender;
  std::mutex _sendMutex;

  std::shared_ptr<protocol::RecvPackage> packageNode;
  net::io_context &ioc;
};
