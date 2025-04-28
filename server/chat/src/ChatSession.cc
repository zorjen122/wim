#include "ChatSession.h"
#include "ChatServer.h"
#include "Const.h"
#include "Logger.h"
#include "Service.h"

#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>

#include "Const.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <climits>

namespace wim {
Tlv::Tlv(unsigned int maxLen, unsigned int msgID) {

  unsigned int tmp = ntohl(maxLen);
  id = ntohl(msgID);
  total = tmp + PROTOCOL_HEADER_TOTAL;

  data = new char[tmp];
}

Tlv::Tlv(unsigned int msgID, unsigned int maxLength, char *msg) {

  id = msgID;

  total = maxLength + PROTOCOL_HEADER_TOTAL;
  data = new char[total];

  unsigned int tmpID = htonl(id);
  unsigned int tmpLen = htonl(maxLength);

  memcpy(data, &tmpID, PROTOCOL_ID_LEN);
  memcpy(data + PROTOCOL_ID_LEN, &tmpLen, PROTOCOL_DATA_SIZE_LEN);
  memcpy(data + PROTOCOL_ID_LEN + PROTOCOL_DATA_SIZE_LEN, msg, maxLength);
}

unsigned int Tlv::getDataSize() { return total - PROTOCOL_HEADER_TOTAL; }
unsigned int Tlv::getTotal() { return total; }

std::string Tlv::getData() { return std::string(data, total); }

Tlv::~Tlv() {
  if (data) {
    delete[] data;
    data = nullptr;
  }
}

NetworkMessage::NetworkMessage(ChatSession::Ptr contextSession,
                               Tlv::Ptr protocolData)
    : contextSession(contextSession), protocolData(protocolData) {}
std::string NetworkMessage::getData() { return protocolData->getData(); };

ChatSession::ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
                         size_t sessionID)
    : id(sessionID), socket(ioContext), chatServer(server), closeEnable(false),
      parseState(WAIT_HEADER), ioContext(ioContext) {
  recvStreamBuffer.prepare(PROTOCOL_DATA_MTU);
  spdlog::info("ChatSession construct, sessionID is {}", sessionID);
}

ChatSession::~ChatSession() { spdlog::info("~ChatSession destruct"); }

void ChatSession::Start() {
  switch (parseState) {
  case WAIT_HEADER: {
    boost::asio::async_read(
        socket, recvStreamBuffer,
        boost::asio::transfer_exactly(PROTOCOL_HEADER_TOTAL),
        [this, self = shared_from_this()](net::error_code ec, size_t) {
          if (ec)
            return HandleError(ec);

          uint32_t currentMsgID = 0;
          uint32_t expectedBodyLen = 0;
          std::istream is(&recvStreamBuffer);
          is.read(reinterpret_cast<char *>(&currentMsgID), sizeof(uint32_t));
          is.read(reinterpret_cast<char *>(&expectedBodyLen), sizeof(uint32_t));

          protocolData = std::make_shared<ChatSession::Protocol>(
              expectedBodyLen, currentMsgID);

          // 验证长度有效性
          if (protocolData->getTotal() > PROTOCOL_RECV_MSS) {
            LOG_WARN(netLogger, "invalid data length is {} > PROTOCOL_RECV_MSS",
                     protocolData->getTotal());
            recvStreamBuffer.consume(PROTOCOL_HEADER_TOTAL);
            parseState = WAIT_HEADER;
            return;
          }
          recvStreamBuffer.consume(PROTOCOL_HEADER_TOTAL);
          parseState = ParseState::WAIT_BODY;
          Start();
        });
    break;
  }
  case WAIT_BODY: {
    // streambuffer将自动分段
    boost::asio::async_read(
        socket, recvStreamBuffer,
        boost::asio::transfer_exactly(protocolData->getDataSize()),
        [this, self = shared_from_this()](net::error_code ec, size_t byte) {
          if (ec)
            return HandleError(ec);

          if (byte != protocolData->getDataSize()) {
            LOG_ERROR(netLogger,
                      "boost::asio::async_read完整读取出现异常, "
                      "数据长度不匹配, 期望长度:{}, 实际长度:{}",
                      byte, protocolData->getDataSize());
            recvStreamBuffer.consume(byte);
            Start();
          }

          memcpy(protocolData->data, recvStreamBuffer.data().data(), byte);

          Service::GetInstance()->PushService(std::shared_ptr<NetworkMessage>(
              new NetworkMessage(shared_from_this(), protocolData)));
          recvStreamBuffer.consume(byte);

          // 回到头解析状态
          parseState = ParseState::WAIT_HEADER;
          Start();
        });
    break;
  }
  default:
    LOG_ERROR(netLogger, "无效的解析状态!");
  }
}
void ChatSession::HandleError(net::error_code ec) {
  /*
    eof：表示对端关闭了连接
    connection_reset：表示对方异常断开连接
  */
  if (ec == net::error::eof) {
    LOG_DEBUG(netLogger, "end of file, socket close | oper-sessionID: {}", id);

  } else if (ec == net::error::connection_reset) {
    LOG_DEBUG(netLogger, "connection reset, socket close | oper-sessionID: {}",
              id);
  } else {
    LOG_DEBUG(netLogger, "handle error, error is {} | oper-sessionID: {}",
              ec.message(), id);
  }

  Close();
  chatServer->ClearSession(id);
}

void ChatSession::Send(char *msgData, unsigned int maxSize,
                       unsigned int msgID) {

  std::lock_guard<std::mutex> lock(sendMutex);
  auto senderSize = sendQueue.size();
  if (senderSize > PROTOCOL_SEND_MSS) {
    LOG_ERROR(netLogger, "session: {} send que fulled, size is {}", id,
              PROTOCOL_SEND_MSS);
    return;
  }

  sendQueue.push(std::make_shared<Tlv>(msgID, maxSize, msgData));
  if (senderSize > 0)
    return;

  // 按<id, total, data>协议回包
  auto &responsePackage = sendQueue.front();

  // 写入操作会发送完整的包数据
  boost::asio::async_write(
      socket,
      boost::asio::buffer(responsePackage->data, responsePackage->total),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                shared_from_this()));
}

void ChatSession::Send(std::string msgData, unsigned int msgID) {
  ChatSession::Send(msgData.data(), msgData.length(), msgID);
}

void ChatSession::Close() {
  socket.close();
  closeEnable = true;
}

void ChatSession::HandleWrite(const net::error_code &ec,
                              ChatSession::Ptr sharedSelf) {
  try {
    std::lock_guard<std::mutex> lock(sendMutex);
    if (ec)
      return HandleError(ec);
    LOG_DEBUG(netLogger, "发送成功,sessionID: {}, 服务ID: {}, 总长: {}", id,
              protocolData->id, protocolData->getTotal());

    sendQueue.pop();

    if (!sendQueue.empty()) {
      auto &responsePackage = sendQueue.front();
      boost::asio::async_write(
          socket,
          boost::asio::buffer(responsePackage->data, responsePackage->total),
          std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                    sharedSelf));
    }

  } catch (std::exception &e) {
    LOG_ERROR(netLogger, "[ChatSession::HandleWrite] Exception code is {}",
              e.what());
  }
}

tcp::socket &ChatSession::GetSocket() { return socket; }

net::io_context &ChatSession::GetIoc() { return ioContext; }

size_t ChatSession::GetSessionID() { return id; }

void ChatSession::ClearSession() { chatServer->ClearSession(id); }
}; // namespace wim