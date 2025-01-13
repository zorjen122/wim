#include "ChatServer.h"

#include "ChatSession.h"
#include "IocPool.h"
#include <spdlog/spdlog.h>

OnlineUserManager::~OnlineUserManager() { _sessionMap.clear(); }

std::shared_ptr<ChatSession> OnlineUserManager::GetSession(int uid) {
  std::lock_guard<std::mutex> lock(_session_mtx);
  auto iter = _sessionMap.find(uid);
  if (iter == _sessionMap.end()) {
    return nullptr;
  }

  return iter->second;
}

void OnlineUserManager::MapUserSession(int uid,
                                       std::shared_ptr<ChatSession> session) {
  std::lock_guard<std::mutex> lock(_session_mtx);
  _sessionMap[uid] = session;
}

void OnlineUserManager::RemoveSession(int uid) {
  auto uid_str = std::to_string(uid);
  // RedisManager::GetInstance()->Del(PREFIX_REDIS_UIP + uid_str);
  {
    std::lock_guard<std::mutex> lock(_session_mtx);
    _sessionMap.erase(uid);
  }
}

OnlineUserManager::OnlineUserManager() {}

ChatServer::ChatServer(boost::asio::io_context &iocContext, unsigned short port)
    : Ioc(iocContext), Port(port),
      Acceptor(iocContext, tcp::endpoint(tcp::v4(), port)) {
  spdlog::info("Server construct listen on port : {}", Port);
}

ChatServer::~ChatServer() {
  spdlog::info("Server destruct listen on port : {}", Port);
}

void ChatServer::Start() {
  auto &ioc = IoContextPool::GetInstance()->GetContext();
  auto newSession = std::make_shared<ChatSession>(ioc, this);
  Acceptor.async_accept(newSession->GetSocket(),
                        std::bind(&ChatServer::HandleAccept, this, newSession,
                                  std::placeholders::_1));
}

void ChatServer::HandleAccept(std::shared_ptr<ChatSession> session,
                              const boost::system::error_code &error) {
  if (!error) {
    session->Start();
    std::lock_guard<mutex> lock(Mutex);

    Sessions.insert(std::make_pair(session->GetSessionId(), session));
  } else {
    spdlog::error("session accept failed, error is {}", error.message());
  }

  ChatServer::Start();
}

void ChatServer::ClearSession(const std::string &uuid) {
  if (Sessions.find(uuid) != Sessions.end()) {
    OnlineUserManager::GetInstance()->RemoveSession(
        Sessions[uuid]->GetUserId());
  }

  std::lock_guard<mutex> lock(Mutex);
  Sessions.erase(uuid);
}
