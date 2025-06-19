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

#define PROTOCOL_DATA_MTU 1500

#define PROTOCOL_FROM_LEN 8
#define PROTOCOL_DEVICE_LEN 2
#define PROTOCOL_ID_LEN 4
#define PROTOCOL_DATA_SIZE_LEN 4
#define PROTOCOL_HEADER_TOTAL                                                  \
  (PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN + PROTOCOL_ID_LEN +                 \
   PROTOCOL_DATA_SIZE_LEN)

#define PROTOCOL_RECV_MSS (10 * 1024 * 1024) // 10MB
#define PROTOCOL_SEND_MSS (10 * 1024 * 1024) // 10MB
#define PROTOCOL_QUEUE_MAX_SIZE (10240)

namespace wim {

using namespace boost::asio;

class ChatServer;
class Session : public std::enable_shared_from_this<Session> {
public:
  using ptr = std::shared_ptr<Session>;
  using tcp = ip::tcp;
  using error_code = boost::system::error_code;
  using endpoint = ip::tcp::endpoint;

  struct context;
  struct protocol;

  Session(io_context &ioContext);

  virtual ~Session();

  tcp::socket &GetSocket();
  uint64_t GetSessionID();
  void SetSessionID(uint64_t id);
  io_context &GetIoc();
  ptr GetSharedSelf();
  bool IsConnected();
  std::string GetEndpointToString();
  endpoint GetEndpoint();

  void Start();
  void Send(std::shared_ptr<protocol> packet);
  void Send(const std::string &data, uint32_t serviceId) {}
  void Close();

  virtual void ClearSession() = 0;
  virtual void HandlePacket(std::shared_ptr<context> context) = 0;

  void HandleWrite(const error_code &ec, Session::ptr sharedSelf);
  void HandleError(error_code ec);

private:
  uint64_t sessionId;
  tcp::socket socket;

  char recvBuffer[PROTOCOL_HEADER_TOTAL + 1];
  bool closeEnable;

  std::queue<std::shared_ptr<protocol>> sendQueue;
  std::mutex sendMutex;

  io_context &ioContext;
};

struct Session::protocol {
  using ptr = std::shared_ptr<Session::protocol>;

  void hton();
  void ntoh();
  ~protocol();
  std::string to_string();
  std::size_t capacity();

  protocol(uint64_t from, uint16_t device, uint32_t id,
           const std::string &data);
  static Session::protocol::ptr to_packet(uint64_t from, uint16_t device,
                                          uint32_t id, const std::string &data);

  protocol() = default;
  protocol(const protocol &) = delete;
  protocol &operator=(const protocol &) = delete;

  uint64_t from{};
  uint16_t device{};
  uint32_t id{};
  uint32_t total{};
  char *data{};
};

struct Session::context {
  using Ptr = std::shared_ptr<context>;

  context(Session::ptr, Session::protocol::ptr);

  Session::ptr session;
  Session::protocol::ptr packet;
};

}; // namespace wim