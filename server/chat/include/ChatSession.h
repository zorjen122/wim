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

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net {
using namespace boost::asio;
using boost::system::error_code;
} // namespace net

class ChatSession;
class Service;

class LogicProtocol;

class Tlv {
  friend class LogicProtocol;
  friend class ChatSession;
  friend class Service;

public:
  Tlv(unsigned int packageLength, unsigned int msgID);
  Tlv(unsigned int msgID, unsigned int maxLength, char *msg);
  ~Tlv();

  std::string getData();

private:
  unsigned int id;
  unsigned int length;
  char *data;
  unsigned int cur;
};

class LogicProtocol {
  friend class ChatSession;
  friend class Service;

public:
  LogicProtocol(std::shared_ptr<ChatSession>, std::shared_ptr<Tlv>);
  std::string getData();

private:
  std::shared_ptr<ChatSession> session;
  std::shared_ptr<Tlv> recvPackage;
};

class ChatServer;
class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
  using RequestPackage = LogicProtocol;
  using Protocol = Tlv;

  ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
              size_t id);
  ~ChatSession();

  tcp::socket &GetSocket();
  size_t GetSessionID();
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

  std::queue<std::shared_ptr<Tlv>> sender;
  std::mutex _sendMutex;

  std::shared_ptr<ChatSession::Protocol> packageNode;
  net::io_context &ioc;
};
