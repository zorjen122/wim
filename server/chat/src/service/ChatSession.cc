#include "ChatSession.h"
#include "ChatServer.h"
#include "Logger.h"
#include "Service.h"

#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>

#include "Const.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/spawn.hpp>
#include <climits>
#include <sys/types.h>

namespace wim {

void ChatSession::protocol::hton() {
  from = htonl(from);
  device = htonl(device);
  id = htonl(id);
  total = htonl(total);
}
void ChatSession::protocol::ntoh() {
  from = ntohl(from);
  device = ntohl(device);
  id = ntohl(id);
  total = ntohl(total);
}

ChatSession::protocol::protocol(uint64_t from, uint16_t device, uint32_t id,
                                const std::string &data)
    : from(from), device(device), id(id), total(data.size()), data(nullptr) {
  if (data.empty()) {
    LOG_WARN(netLogger, "{}:{} 消息数据为空", from, device);
    return;
  }
  total = data.size();
  this->data = new char[total + 1];
  memcpy(this->data, data.c_str(), total);
  this->data[total] = '\0';
}

ChatSession::protocol::ptr
ChatSession::protocol::to_packet(uint64_t from, uint16_t device, uint32_t id,
                                 const std::string &data) {
  if (data.empty()) {
    LOG_WARN(netLogger, "{}:{} 消息数据为空", from, device);
    return nullptr;
  }
  ChatSession::protocol::ptr packet(
      new ChatSession::protocol(from, device, id, data));
  packet->from = from;
  packet->device = device;
  packet->id = id;
  packet->total = data.size() + PROTOCOL_HEADER_TOTAL;
  packet->data = new char[packet->total + 1];

  uint32_t dataSize = data.size();
  memcpy(packet->data, &from, PROTOCOL_FROM_LEN);
  memcpy(packet->data + PROTOCOL_FROM_LEN, &device, PROTOCOL_DEVICE_LEN);
  memcpy(packet->data + PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN, &id,
         PROTOCOL_ID_LEN);
  memcpy(packet->data + PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN +
             PROTOCOL_ID_LEN,
         &dataSize, PROTOCOL_DATA_SIZE_LEN);
  memcpy(packet->data + PROTOCOL_HEADER_TOTAL, data.c_str(), dataSize);

  return packet;
}

ChatSession::protocol::~protocol() {
  if (data)
    delete[] data;
}
std::string ChatSession::protocol::to_string() {
  return std::to_string(from) + "|" + std::to_string(device) + "|" +
         std::to_string(id) + "|" + std::to_string(total) + "|" +
         (data == nullptr ? "" : std::string(data));
}

std::size_t ChatSession::protocol::capacity() {
  return (data == nullptr) ? 0 : strlen(data);
}

Channel::Channel(ChatSession::ptr contextSession,
                 ChatSession::protocol::ptr protocolData)
    : contextSession(contextSession), packet(protocolData) {}

ChatSession::ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
                         size_t sessionId)
    : sessionId(sessionId), socket(ioContext), recvBuffer(), chatServer(server),
      closeEnable(false), sendMutex{}, ioContext(ioContext) {}

ChatSession::~ChatSession() {}

void ChatSession::Start() {
  spawn(socket.get_executor(), [this](yield_context yield) {
    error_code ec;
    while (IsConnected()) {
      protocol::ptr packet(new protocol());

      std::size_t recvHeadLen = async_read(
          socket, buffer(recvBuffer, PROTOCOL_HEADER_TOTAL), yield[ec]);

      if (ec) {
        HandleError(ec);
        continue;
      }

      if (recvHeadLen >= PROTOCOL_HEADER_TOTAL) {
        protocol::ptr packet(new protocol());
        memcpy(&(packet->from), recvBuffer, PROTOCOL_FROM_LEN);
        memcpy(&(packet->device), recvBuffer + PROTOCOL_FROM_LEN,
               PROTOCOL_DEVICE_LEN);
        memcpy(&(packet->id),
               recvBuffer + PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN,
               PROTOCOL_ID_LEN);
        memcpy(&(packet->total),
               recvBuffer + PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN +
                   PROTOCOL_ID_LEN,
               PROTOCOL_DATA_SIZE_LEN);
        packet->ntoh();

        if (packet->total > PROTOCOL_RECV_MSS) {
          LOG_ERROR(netLogger,
                    "[{}] 的消息被丢弃，因其承载已超出最大承载量, "
                    "当前大小({}) > 最大承载量({})",
                    GetEndpointToString(), packet->total, PROTOCOL_RECV_MSS);
          continue;
        }

        packet->data = new char[packet->total + 1];

        std::size_t bytesTransferred = 0;
        ec = error_code();
        while (bytesTransferred < packet->total && !ec) {
          bytesTransferred += socket.async_read_some(
              buffer((char *)(packet->data + bytesTransferred),
                     packet->total - bytesTransferred),
              yield[ec]);
        }
        if (ec) {
          HandleError(ec);
          continue;
        }
        packet->data[packet->total] = '\0';
        LOG_DEBUG(netLogger, "[{}] 收到消息 服务ID: {}, 总长: {}",
                  GetEndpointToString(), getServiceIdString(packet->id),
                  packet->total);

        Channel::Ptr channel =
            std::make_shared<Channel>(shared_from_this(), packet);
        // 传送已序列化为本地字节序的消息包
        Service::GetInstance()->PushService(channel);

        memset(recvBuffer, 0, PROTOCOL_HEADER_TOTAL);
      }
    }
  });
}

void ChatSession::HandleError(error_code ec) {
  /*
    eof：表示对端关闭了连接
    connection_reset：表示对方异常断开连接
  */
  LOG_INFO(netLogger, "[{}] 异常信息: {}", GetEndpointToString(), ec.message());

  Close();
  chatServer->ClearSession(sessionId);
}

void ChatSession::Send(std::shared_ptr<protocol> packet, OrderType flag) {
  std::lock_guard<std::mutex> lock(sendMutex);
  auto senderSize = sendQueue.size();
  if (senderSize > PROTOCOL_QUEUE_MAX_SIZE) {
    LOG_ERROR(netLogger,
              "[{}] 的消息被丢弃，因其承载已超出最大发送队列数量, "
              "当前大小({}) > "
              "最大发送队列数量({})",
              GetEndpointToString(), senderSize, PROTOCOL_QUEUE_MAX_SIZE);
    return;
  }

  if (flag == OrderType::NETWORK)
    packet->hton();
  else
    packet->ntoh();

  sendQueue.push(packet);
  if (senderSize > 0)
    return;

  auto &responsePackage = sendQueue.front();

  // 写入操作会发送完整的包数据
  boost::asio::async_write(
      socket,
      boost::asio::buffer(responsePackage->data, responsePackage->capacity()),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                shared_from_this()));
}

void ChatSession::Close() {
  socket.close();
  closeEnable = true;
}

void ChatSession::HandleWrite(const error_code &ec,
                              ChatSession::ptr sharedSelf) {
  try {
    std::lock_guard<std::mutex> lock(sendMutex);
    if (ec)
      return HandleError(ec);
    LOG_DEBUG(netLogger, "[{}] 发送成功 服务ID: {}, 总长: {}",
              GetEndpointToString(), getServiceIdString(protocolData->id),
              protocolData->capacity());

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
    LOG_ERROR(netLogger, "[{}] 发送失败，异常信息： {}", GetEndpointToString(),
              e.what());
  }
}

ChatSession::tcp::socket &ChatSession::GetSocket() { return socket; }

io_context &ChatSession::GetIoc() { return ioContext; }

size_t ChatSession::GetSessionID() { return sessionId; }

bool ChatSession::IsConnected() { return !closeEnable; }

void ChatSession::ClearSession() {
  if (IsConnected())
    chatServer->ClearSession(sessionId);
}

std::string ChatSession::GetEndpointToString() {
  return socket.remote_endpoint().address().to_string() + ":" +
         std::to_string(socket.remote_endpoint().port());
}

ChatSession::endpoint ChatSession::GetEndpoint() {
  return socket.remote_endpoint();
}
}; // namespace wim