#include "ChatServer.h"

#include <iostream>

#include "ChatSession.h"
#include "IOServicePool.h"
#include "UserManager.h"

ChatServer::ChatServer(boost::asio::io_context &io_context, unsigned short port)
    : _io_context(io_context),
      _port(port),
      _acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
  std::cout << "Server start success, listen on port : " << _port << endl;
}

ChatServer::~ChatServer() {
  std::cout << "Server destruct listen on port : " << _port << endl;
}

void ChatServer::Start() {
  auto &io_context = IOServicePool::GetInstance()->GetIOService();
  auto new_session = std::make_shared<ChatSession>(io_context, this);
  _acceptor.async_accept(new_session->GetSocket(),
                         std::bind(&ChatServer::HandleAccept, this, new_session,
                                   std::placeholders::_1));
}

void ChatServer::HandleAccept(std::shared_ptr<ChatSession> new_session,
                              const boost::system::error_code &error) {
  if (!error) {
    new_session->Start();
    std::lock_guard<mutex> lock(_mutex);

    _sessions.insert(std::make_pair(new_session->GetSessionId(), new_session));
  } else {
    std::cout << "session accept failed, error is " << error.what() << endl;
  }

  Start();
}

void ChatServer::ClearSession(const std::string &uuid) {
  if (_sessions.find(uuid) != _sessions.end()) {
    UserManager::GetInstance()->RemoveSession(_sessions[uuid]->GetUserId());
  }

  {
    std::lock_guard<mutex> lock(_mutex);
    _sessions.erase(uuid);
  }
}
