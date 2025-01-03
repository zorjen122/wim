#include "ChatSession.h"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

#include <iostream>

#include "ChatServer.h"
#include "ServiceSystem.h"
#include "spdlog/spdlog.h"
#include <spdlog/spdlog.h>

ChatSession::ChatSession(boost::asio::io_context &io_context,
                         ChatServer *server)
    : _socket(io_context),
      _server(server),
      _closeEnable(false),
      _b_head_parse(false),
      _user_uid(0) {

  auto uuid = boost::uuids::random_generator()();
  _sessionID = boost::uuids::to_string(uuid);
  // _recv_head_node = std::make_shared<protocol::Package>(PACKAGE_TOTAL_LEN);
}

ChatSession::~ChatSession() { std::cout << "~ChatSession destruct" << endl; }

void ChatSession::Start() { ReceivePackageHead(PACKAGE_TOTAL_LEN); }

void ChatSession::Send(std::string msg, short msgid) {
  std::lock_guard<std::mutex> lock(_sendMutex);
  int send_que_size = _send_que.size();
  if (send_que_size > MAX_SEND_QUEUE_LEN) {
    std::cout << "session: " << _sessionID << " send que fulled, size is "
              << MAX_SEND_QUEUE_LEN << endl;
    return;
  }

  _send_que.push(std::make_shared<protocol::SendPackage>(msg.c_str(),
                                                         msg.length(), msgid));
  if (send_que_size > 0) {
    return;
  }
  auto &package = _send_que.front();
  boost::asio::async_write(
      _socket, boost::asio::buffer(package->data, package->total),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                GetSharedSelf()));
}

void ChatSession::Send(char *msg, short max_length, short msgid) {
  std::lock_guard<std::mutex> lock(_sendMutex);
  int send_que_size = _send_que.size();
  if (send_que_size > MAX_SEND_QUEUE_LEN) {
    std::cout << "session: " << _sessionID << " send que fulled, size is "
              << MAX_SEND_QUEUE_LEN << endl;
    return;
  }

  _send_que.push(
      std::make_shared<protocol::SendPackage>(msg, max_length, msgid));
  if (send_que_size > 0) {
    return;
  }
  auto &package = _send_que.front();
  boost::asio::async_write(
      _socket, boost::asio::buffer(package->data, package->total),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                GetSharedSelf()));
}

void ChatSession::Close() {
  _socket.close();
  _closeEnable = true;
}

std::shared_ptr<ChatSession> ChatSession::GetSharedSelf() {
  return shared_from_this();
}

void ChatSession::ReceivePackageBody(int size) {
  auto self = shared_from_this();
  asyncReadFull(
      size, [self, this, size](const boost::system::error_code &ec,
                                         std::size_t bytesTransfered) {
        try {
          if (ec) {
            spdlog::error("handle read failed, error is {}", ec.what());
            Close();
            _server->ClearSession(_sessionID);
            return;
          }

          if (bytesTransfered < size) {
            std::cout << "read length not match, read [" << bytesTransfered
                      << "] , total [" << size << "]" << endl;
            Close();
            _server->ClearSession(_sessionID);
            return;
          }

          memcpy(_packageNode->data, _data, bytesTransfered);
          _packageNode->cur += bytesTransfered;
          _packageNode->data[_packageNode->total] = '\0';
          
          spdlog::info("receive data is {}", _packageNode->data);

          //此处将消息投递到逻辑队列中
          ServiceSystem::GetInstance()->PushLogicPackage(
              std::make_shared<protocol::LogicPackage>(shared_from_this(),
                                                       _packageNode));
          //继续监听头部接受事件
          ReceivePackageHead(PACKAGE_TOTAL_LEN);
        } catch (std::exception &e) {
          spdlog::error("Exception code is {}" , e.what());
        }
      });
}

void ChatSession::ReceivePackageHead(int size) {
  auto self = shared_from_this();
  asyncReadFull(PACKAGE_TOTAL_LEN, [self, this](
                                    const boost::system::error_code &ec,
                                    std::size_t bytesTransfered) {
    try {
      if (ec) {
        spdlog::error("handle read failed, error is {}" , ec.what());
        Close();
        _server->ClearSession(_sessionID);
        return;
      }

      if (bytesTransfered < PACKAGE_TOTAL_LEN) {
        std::cout << "read length not match, read [" << bytesTransfered
                  << "] , total [" << PACKAGE_TOTAL_LEN << "]" << endl;
        Close();
        _server->ClearSession(_sessionID);
        return;
      }

      short packageLen = 0, packageID = 0;
      memcpy(&packageID, _data, PACKAGE_ID_LEN);
      packageID = boost::asio::detail::socket_ops::network_to_host_short(packageID);
      
      // ID非法
      if (packageID > PACKAGE_MAX_LENGTH) {
        spdlog::info("invalid packageID is  {}", packageID);
        _server->ClearSession(_sessionID);
        return;
      }

      memcpy(&packageLen, _data + PACKAGE_ID_LEN, PACKAGE_DATA_LEN);
      packageLen = boost::asio::detail::socket_ops::network_to_host_short(packageLen);

      // 长度非法
      if (packageLen > PACKAGE_MAX_LENGTH) {
        spdlog::info("invalid data length is {}  ", packageLen);
        _server->ClearSession(_sessionID);
        return;
      }
      spdlog::info("receive packageID is {}, packageLen is {} ");

      _packageNode = std::make_shared<protocol::RecvPackage>(packageLen, packageID);
      ReceivePackageBody(packageLen);
    } catch (std::exception &e) {
      spdlog::error( "Exception code is {}", e.what());
    }
  });
}

void ChatSession::HandleWrite(const boost::system::error_code &error,
                              std::shared_ptr<ChatSession> shared_self) {
  // 增加异常处理
  try {
    if (!error) {
      std::lock_guard<std::mutex> lock(_sendMutex);
      // std::cout << "send data " << _send_que.front()->data+HEAD_LENGTH <<
      // endl;
      _send_que.pop();
      if (!_send_que.empty()) {
        auto &package = _send_que.front();
        boost::asio::async_write(
            _socket, boost::asio::buffer(package->data, package->total),
            std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                      shared_self));
      }
    } else {
      std::cout << "handle write failed, error is " << error.what() << endl;
      Close();
      _server->ClearSession(_sessionID);
    }
  } catch (std::exception &e) {
    std::cerr << "Exception code : " << e.what() << endl;
  }
}

// 读取完整长度
void ChatSession::asyncReadFull(
    std::size_t maxLength,
    std::function<void(const boost::system::error_code &, std::size_t)>
        handler) {
  ::memset(_data, 0, PACKAGE_MAX_LENGTH);
  asyncReadLen(0, maxLength, handler);
}

// 读取指定字节数
void ChatSession::asyncReadLen(
    std::size_t read_len, std::size_t total_len,
    std::function<void(const boost::system::error_code &, std::size_t)>
        handler) {
  auto self = shared_from_this();
  _socket.async_read_some(
      boost::asio::buffer(_data + read_len, total_len - read_len),
      [read_len, total_len, handler, self](const boost::system::error_code &ec,
                                           std::size_t bytesTransfered) {
        if (ec) {
          // 出现错误，调用回调函数
          handler(ec, read_len + bytesTransfered);
          return;
        }

        if (read_len + bytesTransfered >= total_len) {
          // 长度够了就调用回调函数
          handler(ec, read_len + bytesTransfered);
          return;
        }

        // 没有错误，且长度不足则继续读取
        self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
      });
}

tcp::socket &ChatSession::GetSocket() { return _socket; }

std::string &ChatSession::GetSessionId() { return _sessionID; }

void ChatSession::SetUserId(int uid) { _user_uid = uid; }

int ChatSession::GetUserId() { return _user_uid; }
