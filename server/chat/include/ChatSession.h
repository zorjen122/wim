#pragma once
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/streambuf.hpp>
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

namespace wim {

class NetworkMessage;

class ChatSession;

class Service;

class Tlv {
  friend class NetworkMessage;
  friend class ChatSession;
  friend class Service;

public:
  using Ptr = std::shared_ptr<Tlv>;

  Tlv(unsigned int packageLength, unsigned int msgID);
  Tlv(unsigned int msgID, unsigned int maxLength, char *msg);
  ~Tlv();

  std::string getData();
  unsigned int getTotal();
  unsigned int getDataSize();

private:
  unsigned int id;
  unsigned int length;
  char *data;
};

class ChatServer;

class ChatSession : public std::enable_shared_from_this<ChatSession> {
private:
  friend class TestChatSession;

public:
  using Protocol = Tlv;
  using Ptr = std::shared_ptr<ChatSession>;

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
  void ClearSession();
  Ptr GetSharedSelf();

private:
  enum ParseState { WAIT_HEADER, WAIT_BODY };

  void HandleWrite(const net::error_code &ec, ChatSession::Ptr sharedSelf);
  void HandleError(net::error_code ec);

private:
  size_t id;
  tcp::socket socket;

  char recvBuffer[PROTOCOL_DATA_MTU];
  net::streambuf recvStreamBuffer;

  ChatServer *chatServer;
  bool closeEnable;

  ParseState parseState;

  std::queue<Tlv::Ptr> sendQueue;
  std::mutex sendMutex;

  Tlv::Ptr protocolData;
  net::io_context &ioContext;
};

class NetworkMessage {
  friend class ChatSession;
  friend class Service;

public:
  using Ptr = std::shared_ptr<NetworkMessage>;

  NetworkMessage(ChatSession::Ptr, Tlv::Ptr);
  std::string getData();

private:
  ChatSession::Ptr contextSession;
  Tlv::Ptr protocolData;
};

}; // namespace wim