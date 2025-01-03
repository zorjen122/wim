#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <mutex>
#include <queue>

#include "Const.h"
#include "Protocol.h"
using namespace std;

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

class ChatServer;
class ServiceSystem;

class ChatSession : public std::enable_shared_from_this<ChatSession> {
 public:
  ChatSession(boost::asio::io_context &io_context, ChatServer *server);
  ~ChatSession();
  tcp::socket &GetSocket();
  std::string &GetSessionId();
  void SetUserId(int uid);
  int GetUserId();
  void Start();
  void Send(char *msg, short max_length, short msgid);
  void Send(std::string msg, short msgid);
  void Close();
  std::shared_ptr<ChatSession> GetSharedSelf();
  void ReceivePackageBody(int length);
  void ReceivePackageHead(int total_len);

 private:
  void asyncReadFull(
      std::size_t maxLength,
      std::function<void(const boost::system::error_code &, std::size_t)>
          handler);
  void asyncReadLen(
      std::size_t read_len, std::size_t total_len,
      std::function<void(const boost::system::error_code &, std::size_t)>
          handler);

  void HandleWrite(const boost::system::error_code &error,
                   std::shared_ptr<ChatSession> shared_self);

 private:
  tcp::socket _socket;
  std::string _sessionID;
  char _data[PACKAGE_MAX_LENGTH];
  ChatServer *_server;
  bool _closeEnable;
  std::queue<std::shared_ptr<protocol::SendPackage>> _send_que;
  std::mutex _sendMutex;

  std::shared_ptr<protocol::RecvPackage> _packageNode;
  bool _b_head_parse;

  std::shared_ptr<protocol::Package> _recv_head_node;
  int _user_uid;
};
