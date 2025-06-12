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

class Channel;

class ChatSession;

class Service;

class Tlv {
  friend class Channel;
  friend class ChatSession;
  friend class Service;

public:
  using Ptr = std::shared_ptr<Tlv>;

  // 从网络字节序数据转换到本地字节序
  Tlv(uint32_t packageLength, uint32_t msgID);

  // 从本地字节序数据转换到网络字节序
  Tlv(uint32_t msgID, uint32_t maxLength, char *msg);
  ~Tlv();

  void setData(const char *msg, uint32_t msgLength);
  std::string getData();
  uint32_t getTotal();
  uint32_t getDataSize();

private:
  uint32_t id = 0;
  uint32_t total = 0;
  char *data = nullptr;
};

class ChatServer;

class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
  using Protocol = Tlv;
  using Ptr = std::shared_ptr<ChatSession>;

  ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
              uint64_t sessionId);
  ~ChatSession();

  tcp::socket &GetSocket();
  uint64_t GetSessionID();
  net::io_context &GetIoc();

  void Start();
  void Send(char *msgData, uint32_t msgLength, uint32_t msgID);
  void Send(std::string msgData, uint32_t msgID);
  void Close();
  void ClearSession();
  Ptr GetSharedSelf();

  bool IsConnected();

private:
  enum ParseState { WAIT_HEADER, WAIT_BODY };

  void HandleWrite(const net::error_code &ec, ChatSession::Ptr sharedSelf);
  void HandleError(net::error_code ec);

private:
  uint64_t sessionId;
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

class Channel {
  friend class ChatSession;
  friend class Service;

public:
  using Ptr = std::shared_ptr<Channel>;

  Channel(ChatSession::Ptr, Tlv::Ptr);
  std::string getData();

private:
  ChatSession::Ptr contextSession;
  Tlv::Ptr protocolData;
};

}; // namespace wim