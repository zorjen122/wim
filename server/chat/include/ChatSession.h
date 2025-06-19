#pragma once
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <sys/types.h>
#include <unistd.h>

#include "Const.h"

namespace wim {

using namespace boost::asio;

class ChatServer;
class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
  using ptr = std::shared_ptr<ChatSession>;
  using tcp = ip::tcp;
  using error_code = boost::system::error_code;
  using endpoint = ip::tcp::endpoint;

  struct protocol;

  ChatSession(io_context &ioContext, ChatServer *server, uint64_t sessionId);
  ~ChatSession();

  tcp::socket &GetSocket();
  uint64_t GetSessionID();
  io_context &GetIoc();
  ptr GetSharedSelf();
  bool IsConnected();
  std::string GetEndpointToString();
  endpoint GetEndpoint();

  void Start();
  enum OrderType { NETWORK, HOST };
  void Send(std::shared_ptr<protocol> packet,
            OrderType flag = OrderType::NETWORK);
  void Send(const std::string &data, uint32_t serviceId) {}
  void Close();
  void ClearSession();

private:
  void HandleWrite(const error_code &ec, ChatSession::ptr sharedSelf);
  void HandleError(error_code ec);

private:
  uint64_t sessionId;
  tcp::socket socket;

  char recvBuffer[PROTOCOL_HEADER_TOTAL + 1];

  ChatServer *chatServer;
  bool closeEnable;

  std::queue<std::shared_ptr<protocol>> sendQueue;
  std::mutex sendMutex;

  std::shared_ptr<protocol> protocolData;
  io_context &ioContext;
};

struct ChatSession::protocol {
  using ptr = std::shared_ptr<ChatSession::protocol>;

  void hton();
  void ntoh();
  ~protocol();
  std::string to_string();
  std::size_t capacity();

  protocol(uint64_t from, uint16_t device, uint32_t id,
           const std::string &data);
  static ChatSession::protocol::ptr to_packet(uint64_t from, uint16_t device,
                                              uint32_t id,
                                              const std::string &data);

  protocol() = default;
  protocol(const protocol &) = delete;
  protocol &operator=(const protocol &) = delete;

  uint64_t from{};
  uint16_t device{};
  uint32_t id{};
  uint32_t total{};
  char *data{};
};

class Channel {
  friend class ChatSession;
  friend class Service;

public:
  using Ptr = std::shared_ptr<Channel>;

  Channel(ChatSession::ptr, ChatSession::protocol::ptr);

private:
  ChatSession::ptr contextSession;
  ChatSession::protocol::ptr packet;
};

}; // namespace wim