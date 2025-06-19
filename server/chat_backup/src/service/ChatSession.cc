#include "ChatSession.h"
#include "ChatServer.h"
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
Tlv::Tlv(uint32_t maxLen, uint32_t msgID) {

  uint32_t tmp = ntohl(maxLen);
  id = ntohl(msgID);
  total = tmp + PROTOCOL_HEADER_TOTAL;

  data = new char[tmp];
}

Tlv::Tlv(uint32_t msgID, uint32_t maxLength, char *msg) {

  id = msgID;

  total = maxLength + PROTOCOL_HEADER_TOTAL;
  data = new char[total];

  uint32_t tmpID = htonl(id);
  uint32_t tmpLen = htonl(maxLength);

  memcpy(data, &tmpID, PROTOCOL_ID_LEN);
  memcpy(data + PROTOCOL_ID_LEN, &tmpLen, PROTOCOL_DATA_SIZE_LEN);
  memcpy(data + PROTOCOL_ID_LEN + PROTOCOL_DATA_SIZE_LEN, msg, maxLength);
}
void Tlv::setData(const char *msg, uint32_t msgLength) {
  memcpy(data, msg, msgLength);
}

uint32_t Tlv::getDataSize() { return total - PROTOCOL_HEADER_TOTAL; }
uint32_t Tlv::getTotal() { return total; }

std::string Tlv::getData() { return std::string(data, total); }

Tlv::~Tlv() {
  if (data) {
    delete[] data;
    data = nullptr;
  }
}

Channel::Channel(ChatSession::Ptr contextSession, Tlv::Ptr protocolData)
    : contextSession(contextSession), protocolData(protocolData) {}
std::string Channel::getData() { return protocolData->getData(); };

ChatSession::ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
                         size_t sessionId)
    : sessionId(sessionId), socket(ioContext), chatServer(server),
      closeEnable(false), parseState(WAIT_HEADER), sendMutex{},
      ioContext(ioContext) {
  recvStreamBuffer.prepare(PROTOCOL_DATA_MTU);
}

ChatSession::~ChatSession() {}

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

          // 验证长度有效性
          uint32_t temp = htons(expectedBodyLen);
          if (temp > PROTOCOL_RECV_MSS) {
            LOG_WARN(netLogger,
                     "【{}】无效长度，超出了协议规定的最大长度: {} > "
                     "PROTOCOL_RECV_MSS({})",
                     GetEndpointToString(), temp, PROTOCOL_RECV_MSS);
            recvStreamBuffer.consume(PROTOCOL_HEADER_TOTAL);
            parseState = WAIT_HEADER;
            return;
          }

          protocolData = std::make_shared<ChatSession::Protocol>(
              expectedBodyLen, currentMsgID);

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
                      "【{}】boost::asio::async_read完整读取出现异常, "
                      "数据长度和预期长度不匹配, 预期长度:{}, 实际长度:{}",
                      GetEndpointToString(), byte, protocolData->getDataSize());
            recvStreamBuffer.consume(byte);
            Start();
          }

          protocolData->setData(
              static_cast<const char *>(recvStreamBuffer.data().data()), byte);

          Service::GetInstance()->PushService(std::shared_ptr<Channel>(
              new Channel(shared_from_this(), protocolData)));
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
  LOG_INFO(netLogger, "【{}】异常信息: {}", GetEndpointToString(),
           ec.message());

  Close();
  chatServer->ClearSession(sessionId);
}

void ChatSession::Send(char *msgData, uint32_t maxSize, uint32_t msgID) {

  std::lock_guard<std::mutex> lock(sendMutex);
  auto senderSize = sendQueue.size();
  if (senderSize > PROTOCOL_QUEUE_MAX_SIZE) {
    LOG_ERROR(netLogger,
              "【{}】的消息被丢弃，因其承载已超出最大发送队列数量, "
              "当前大小({}) > "
              "最大发送队列数量({})",
              GetEndpointToString(), senderSize, PROTOCOL_QUEUE_MAX_SIZE);
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

void ChatSession::Send(std::string msgData, uint32_t msgID) {
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
    LOG_DEBUG(netLogger, "【{}】发送成功 服务ID: {}, 总长: {}",
              GetEndpointToString(), getServiceIdString(protocolData->id),
              protocolData->getTotal());

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
    LOG_ERROR(netLogger, "【{}】发送失败，异常信息： {}", GetEndpointToString(),
              e.what());
  }
}

tcp::socket &ChatSession::GetSocket() { return socket; }

net::io_context &ChatSession::GetIoc() { return ioContext; }

size_t ChatSession::GetSessionID() { return sessionId; }

void ChatSession::ClearSession() {
  if (IsConnected())
    chatServer->ClearSession(sessionId);
}

bool ChatSession::IsConnected() { return !closeEnable; }

std::string ChatSession::GetEndpointToString() {
  return socket.remote_endpoint().address().to_string() + ":" +
         std::to_string(socket.remote_endpoint().port());
}

net::ip::tcp::endpoint ChatSession::GetEndpoint() {
  return socket.remote_endpoint();
}

}; // namespace wim