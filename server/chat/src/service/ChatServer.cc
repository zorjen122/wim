#include "ChatServer.h"

#include "ChatSession.h"
#include "IocPool.h"
#include "Logger.h"
#include "OnlineUser.h"
#include <mutex>
#include <spdlog/spdlog.h>
namespace wim {
ChatServer::ChatServer(boost::asio::io_context &iocContext, unsigned short port)
    : Ioc(iocContext), Port(port),
      Acceptor(iocContext, tcp::endpoint(tcp::v4(), port)) {
  LOG_INFO(netLogger, "Server construct listen on port : {}", Port);
}

ChatServer::~ChatServer() {
  LOG_INFO(netLogger, "Server destruct listen on port : {}", Port);
}

void ChatServer::Start() {
  auto &ioc = IocPool::GetInstance()->GetContext();
  ++sessionID;
  auto newSession = std::make_shared<ChatSession>(ioc, this, sessionID);
  Acceptor.async_accept(newSession->GetSocket(),
                        std::bind(&ChatServer::HandleAccept, this, newSession,
                                  std::placeholders::_1));
}

void ChatServer::HandleAccept(ChatSession::Ptr session,
                              const boost::system::error_code &error) {
  if (!error) {
    session->Start();
    std::lock_guard<std::mutex> lock(Mutex);
    sessionGroup[sessionID] = session;
  } else {
    LOG_ERROR(netLogger,
              "[ChatServer::HandleAccept] session accept failed, error is {}",
              error.message());
  }

  ChatServer::Start();
}

void ChatServer::ClearSession(size_t id) {
  if (sessionGroup.find(id) == sessionGroup.end()) {
    LOG_ERROR(netLogger,
              "[ChatServer::ClearSession] session not found, key is {}", id);
    return;
  }
  sessionGroup.erase(id);
}

size_t ChatServer::GetSessionID() { return sessionID; }
}; // namespace wim