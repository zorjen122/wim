#include "ChatServer.h"

#include "ChatSession.h"
#include "IocPool.h"
#include <mutex>
#include <spdlog/spdlog.h>

ChatServer::ChatServer(boost::asio::io_context &iocContext, unsigned short port)
    : Ioc(iocContext), Port(port),
      Acceptor(iocContext, tcp::endpoint(tcp::v4(), port)) {
  spdlog::info("Server construct listen on port : {}", Port);
}

ChatServer::~ChatServer() {
  spdlog::info("Server destruct listen on port : {}", Port);
}

void ChatServer::Start() {
  auto &ioc = IocPool::GetInstance()->GetContext();
  ++count;
  auto newSession = std::make_shared<ChatSession>(ioc, this, count);
  Acceptor.async_accept(newSession->GetSocket(),
                        std::bind(&ChatServer::HandleAccept, this, newSession,
                                  std::placeholders::_1));
}

void ChatServer::HandleAccept(std::shared_ptr<ChatSession> session,
                              const boost::system::error_code &error) {
  if (!error) {
    session->Start();
    std::lock_guard<std::mutex> lock(Mutex);
  } else {
    spdlog::error("session accept failed, error is {}", error.message());
  }

  ChatServer::Start();
}

void ChatServer::ClearSession(size_t id) {
  if (sessionGroup.find(id) == sessionGroup.end()) {
    spdlog::error("[ChatServer::ClearSession] session not found, key is {}",
                  id);
    return;
  }
  sessionGroup.erase(id);
}
