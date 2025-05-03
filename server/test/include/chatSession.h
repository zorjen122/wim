#pragma once
#include "Logger.h"
#include "global.h"
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
#include <unistd.h>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net {
using namespace boost::asio;
using boost::system::error_code;
} // namespace net

namespace wim {

class Tlv {
  friend class Chat;
  friend class ChatSession;

public:
  using Ptr = std::shared_ptr<Tlv>;

  Tlv(unsigned int packageLength, unsigned int msgID);
  Tlv(unsigned int msgID, unsigned int maxLength, char *msg);
  ~Tlv();

  std::string getDataString();
  char *getData();
  unsigned int getTotal();
  unsigned int getDataSize();

private:
  unsigned int id;
  unsigned int length;
  char *data;
};

class Chat;
class ChatSession : public std::enable_shared_from_this<ChatSession> {
private:
  friend class TestChatSession;

public:
  using Protocol = Tlv;
  using Ptr = std::shared_ptr<ChatSession>;

  ChatSession(net::io_context &iocontext, Endpoint endpoint);
  ~ChatSession();

  bool isConnected();

  void Start();
  void Send(char *msgData, unsigned int msgLength, unsigned int msgID);
  void Send(std::string msgData, unsigned int msgID);
  void Close();
  void ClearSession();
  Ptr GetSharedSelf() { return shared_from_this(); }
  net::io_context &getIoContext() { return iocontext; }

  enum ParseState { WAIT_HEADER, WAIT_BODY };

  void HandleError(net::error_code ec);

  std::shared_ptr<tcp::socket> chat;

  net::streambuf recvStreamBuffer;

  bool closeEnable;

  ParseState parseState;

  Tlv::Ptr sendProtocolData;
  std::mutex sendMutex;

  Tlv::Ptr recvProtocolData;
  net::io_context &iocontext;
};

} // namespace wim