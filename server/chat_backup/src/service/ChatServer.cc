#include "ChatServer.h"

#include "ChatSession.h"
#include "IocPool.h"
#include "Logger.h"
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>

namespace wim {
ChatServer::ChatServer(boost::asio::io_context &acceptContext, uint16_t port)
    : acceptContext(acceptContext), Port(port),
      Acceptor(acceptContext, tcp::endpoint(tcp::v4(), port)), sessionID(0) {
  LOG_INFO(netLogger, "ChatServer通讯服务启动监听，端口号为: {}", Port);
}

ChatServer::~ChatServer() {
  LOG_INFO(netLogger, "ChatServer通讯服务停止监听，端口号为: {}", Port);
}

void ChatServer::Start() {
  auto &ioc = IocPool::GetInstance()->GetContext();
  auto newSession = std::make_shared<ChatSession>(ioc, this, (sessionID++));
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

    LOG_INFO(netLogger, "连接成功，客户端主机地址: {}",
             session->GetEndpointToString());
  } else {
    LOG_WARN(netLogger, "连接发生错误，错误信息为: {}", error.message());
  }

  ChatServer::Start();
}

void ChatServer::ClearSession(uint64_t id) {
  std::lock_guard<std::mutex> lock(Mutex);
  if (sessionGroup.find(id) == sessionGroup.end()) {
    LOG_ERROR(netLogger, "没有这样的会话，会话ID为： {}", id);
    return;
  }
  sessionGroup.erase(id);
}

uint64_t ChatServer::GetSessionID() { return sessionID; }
}; // namespace wim