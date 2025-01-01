#include "ChatSession.h"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

#include <iostream>
#include <sstream>

#include "ChatServer.h"
#include "ServiceSystem.h"
#include "test.h"

ChatSession::ChatSession(boost::asio::io_context &io_context,
                         ChatServer *server)
    : _socket(io_context),
      _server(server),
      _b_close(false),
      _b_head_parse(false),
      _user_uid(0) {
#ifdef TEST_IM
  __test::idMap[__test::sessionId] = shared_from_this();
  __test::idGroup.push_back(__test::sessionId);
  __test::sessionId++;
#else

  boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
  _session_id = boost::uuids::to_string(a_uuid);
  _recv_head_node = std::make_shared<protocol::Package>(HEAD_TOTAL_LEN);
#endif
}

ChatSession::~ChatSession() { std::cout << "~ChatSession destruct" << endl; }

void ChatSession::Start() { AsyncReadHead(HEAD_TOTAL_LEN); }

void ChatSession::Send(std::string msg, short msgid) {
  std::lock_guard<std::mutex> lock(_send_lock);
  int send_que_size = _send_que.size();
  if (send_que_size > MAX_SEND_QUEUE_LEN) {
    std::cout << "session: " << _session_id << " send que fulled, size is "
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
      _socket, boost::asio::buffer(package->data, package->total_len),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                SharedSelf()));
}

void ChatSession::Send(char *msg, short max_length, short msgid) {
  std::lock_guard<std::mutex> lock(_send_lock);
  int send_que_size = _send_que.size();
  if (send_que_size > MAX_SEND_QUEUE_LEN) {
    std::cout << "session: " << _session_id << " send que fulled, size is "
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
      _socket, boost::asio::buffer(package->data, package->total_len),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                SharedSelf()));
}

void ChatSession::Close() {
  _socket.close();
  _b_close = true;
}

std::shared_ptr<ChatSession> ChatSession::SharedSelf() {
  return shared_from_this();
}

void ChatSession::AsyncReadBody(int total_len) {
  auto self = shared_from_this();
  asyncReadFull(
      total_len, [self, this, total_len](const boost::system::error_code &ec,
                                         std::size_t bytes_transfered) {
        try {
          if (ec) {
            std::cout << "handle read failed, error is " << ec.what() << endl;
            Close();
            _server->ClearSession(_session_id);
            return;
          }

          if (bytes_transfered < total_len) {
            std::cout << "read length not match, read [" << bytes_transfered
                      << "] , total [" << total_len << "]" << endl;
            Close();
            _server->ClearSession(_session_id);
            return;
          }

          memcpy(_recv_msg_node->data, _data, bytes_transfered);
          _recv_msg_node->cur_len += bytes_transfered;
          _recv_msg_node->data[_recv_msg_node->total_len] = '\0';
          std::cout << "receive data is " << _recv_msg_node->data << endl;
          //此处将消息投递到逻辑队列中
          ServiceSystem::GetInstance()->PushLogicPackage(
              std::make_shared<protocol::LogicPackage>(shared_from_this(),
                                                       _recv_msg_node));
          //继续监听头部接受事件
          AsyncReadHead(HEAD_TOTAL_LEN);
        } catch (std::exception &e) {
          std::cout << "Exception code is " << e.what() << endl;
        }
      });
}

void ChatSession::AsyncReadHead(int total_len) {
  auto self = shared_from_this();
  asyncReadFull(HEAD_TOTAL_LEN, [self, this](
                                    const boost::system::error_code &ec,
                                    std::size_t bytes_transfered) {
    try {
      if (ec) {
        std::cout << "handle read failed, error is " << ec.what() << endl;
        Close();
        _server->ClearSession(_session_id);
        return;
      }

      if (bytes_transfered < HEAD_TOTAL_LEN) {
        std::cout << "read length not match, read [" << bytes_transfered
                  << "] , total [" << HEAD_TOTAL_LEN << "]" << endl;
        Close();
        _server->ClearSession(_session_id);
        return;
      }

      _recv_head_node->Clear();
      memcpy(_recv_head_node->data, _data, bytes_transfered);

      //获取头部MSGID数据
      short msg_id = 0;
      memcpy(&msg_id, _recv_head_node->data, HEAD_ID_LEN);
      //网络字节序转化为本地字节序
      msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
      std::cout << "msg_id is " << msg_id << endl;
      // id非法
      if (msg_id > MAX_LENGTH) {
        std::cout << "invalid msg_id is " << msg_id << endl;
        _server->ClearSession(_session_id);
        return;
      }
      short msg_len = 0;
      memcpy(&msg_len, _recv_head_node->data + HEAD_ID_LEN, HEAD_DATA_LEN);
      //网络字节序转化为本地字节序
      msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
      std::cout << "msg_len is " << msg_len << endl;

      // id非法
      if (msg_len > MAX_LENGTH) {
        std::cout << "invalid data length is " << msg_len << endl;
        _server->ClearSession(_session_id);
        return;
      }

      _recv_msg_node = std::make_shared<protocol::RecvPackage>(msg_len, msg_id);
      AsyncReadBody(msg_len);
    } catch (std::exception &e) {
      std::cout << "Exception code is " << e.what() << endl;
    }
  });
}

void ChatSession::HandleWrite(const boost::system::error_code &error,
                              std::shared_ptr<ChatSession> shared_self) {
  // 增加异常处理
  try {
    if (!error) {
      std::lock_guard<std::mutex> lock(_send_lock);
      // std::cout << "send data " << _send_que.front()->data+HEAD_LENGTH <<
      // endl;
      _send_que.pop();
      if (!_send_que.empty()) {
        auto &package = _send_que.front();
        boost::asio::async_write(
            _socket, boost::asio::buffer(package->data, package->total_len),
            std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                      shared_self));
      }
    } else {
      std::cout << "handle write failed, error is " << error.what() << endl;
      Close();
      _server->ClearSession(_session_id);
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
  ::memset(_data, 0, MAX_LENGTH);
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

std::string &ChatSession::GetSessionId() { return _session_id; }

void ChatSession::SetUserId(int uid) { _user_uid = uid; }

int ChatSession::GetUserId() { return _user_uid; }
