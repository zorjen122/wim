#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

#include "Const.h"
#include "Protocol.h"
#include "Timer.h"
using namespace std;

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

class ChatServer;
class ServiceSystem;

class ChatSession : public std::enable_shared_from_this<ChatSession>,
                    public Timer {
public:
  ChatSession(boost::asio::io_context &ioContext, ChatServer *server);
  ~ChatSession();

  tcp::socket &GetSocket();
  std::string &GetSessionId();

  void SetUserId(int uid);
  int GetUserId();
  void Start();
  void Send(char *msgData, short msgLength, short msgID);
  void Send(std::string msgData, short msgID);
  void Close();
  std::shared_ptr<ChatSession> GetSharedSelf();
  void ReceiveBody(size_t size);
  void ReceiveHead(size_t size);

  bool resetHandle() override;
  void tickleHandle() override;

private:
  void asyncReadFull(
      std::size_t maxLength,
      std::function<void(const boost::system::error_code &, std::size_t)>
          handler);
  void asyncReadLen(
      std::size_t readLen, std::size_t total,
      std::function<void(const boost::system::error_code &, std::size_t)>
          handler);

  void HandleWrite(const boost::system::error_code &error,
                   std::shared_ptr<ChatSession> sharedSelf);

private:
  tcp::socket sock;
  std::string sessionID;
  char _data[PACKAGE_MAX_LENGTH];
  ChatServer *chatServer;
  bool closeEnable;
  std::queue<std::shared_ptr<protocol::SendPackage>> sender;
  std::mutex _sendMutex;

  std::shared_ptr<protocol::RecvPackage> packageNode;

  std::shared_ptr<protocol::Package> _recv_head_node;
  int _serviceUID;

  bool _hasPing;
};
