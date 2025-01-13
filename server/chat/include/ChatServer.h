#pragma once
#include <memory.h>

#include <boost/asio.hpp>
#include <map>
#include <mutex>

#include "ChatSession.h"

using boost::asio::ip::tcp;

#include <memory>
#include <mutex>
#include <unordered_map>

class ChatSession;
class OnlineUserManager : public Singleton<OnlineUserManager> {
  friend class Singleton<OnlineUserManager>;

public:
  ~OnlineUserManager();
  std::shared_ptr<ChatSession> GetSession(int uid);
  void MapUserSession(int uid, std::shared_ptr<ChatSession> session);
  void RemoveSession(int uid);

private:
  OnlineUserManager();
  std::mutex _session_mtx;
  std::unordered_map<int, std::shared_ptr<ChatSession>> _sessionMap;
};

class ChatServer {
public:
  ChatServer() = delete;
  ChatServer(boost::asio::io_context &ioContext, unsigned short port);
  ~ChatServer();
  void ClearSession(const std::string &uuid);
  void Start();

private:
  void HandleAccept(std::shared_ptr<ChatSession>,
                    const boost::system::error_code &error);

private:
  boost::asio::io_context &Ioc;
  unsigned short Port;
  tcp::acceptor Acceptor;
  std::map<std::string, std::shared_ptr<ChatSession>> Sessions;
  std::mutex Mutex;
};
