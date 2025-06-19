#include "Session.h"

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

#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/spawn.hpp>
#include <climits>
#include <sys/types.h>

namespace wim {

void Session::protocol::hton() {
  from = htonl(from);
  device = htonl(device);
  id = htonl(id);
  total = htonl(total);
}
void Session::protocol::ntoh() {
  from = ntohl(from);
  device = ntohl(device);
  id = ntohl(id);
  total = ntohl(total);
}

Session::protocol::protocol(uint64_t from, uint16_t device, uint32_t id,
                            const std::string &data)
    : from(from), device(device), id(id), total(data.size()), data(nullptr) {
  if (data.empty()) {
    spdlog::info("{}:{} 消息数据为空", from, device);
    return;
  } else if (total > PROTOCOL_RECV_MSS) {
    spdlog::info("{}:{} 消息数据过大, 超过最大承载量, 丢弃", from, device);
  }
  total = data.size();
  this->data = new char[total + 1];
  memcpy(this->data, data.c_str(), total);
  this->data[total] = '\0';
}

Session::protocol::ptr Session::protocol::to_packet(uint64_t from,
                                                    uint16_t device,
                                                    uint32_t id,
                                                    const std::string &data) {
  if (data.empty()) {
    spdlog::info("{}:{} 消息数据为空", from, device);
    return nullptr;
  } else if (data.size() > PROTOCOL_RECV_MSS) {
    spdlog::info("{}:{} 消息数据过大, 超过最大承载量, 丢弃", from, device);
    return nullptr;
  }

  Session::protocol::ptr packet(new Session::protocol());
  packet->from = htonl(from);
  packet->device = htonl(device);
  packet->id = htonl(id);
  packet->total = PROTOCOL_HEADER_TOTAL + data.size();
  packet->data = new char[packet->total + 1];
  uint32_t dataSize = htonl(data.size());

  memcpy(packet->data, &from, PROTOCOL_FROM_LEN);
  memcpy(packet->data + PROTOCOL_FROM_LEN, &device, PROTOCOL_DEVICE_LEN);
  memcpy(packet->data + PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN, &id,
         PROTOCOL_ID_LEN);
  memcpy(packet->data + PROTOCOL_FROM_LEN + PROTOCOL_DEVICE_LEN +
             PROTOCOL_ID_LEN,
         &dataSize, PROTOCOL_DATA_SIZE_LEN);
  memcpy(packet->data + PROTOCOL_HEADER_TOTAL, data.c_str(), data.size());

  packet->data[packet->total] = '\0';
  return packet;
}

Session::protocol::~protocol() {
  if (data)
    delete[] data;
}
std::string Session::protocol::to_string() {
  return std::to_string(from) + "|" + std::to_string(device) + "|" +
         std::to_string(id) + "|" + std::to_string(total) + "|" +
         std::to_string(capacity()) + " | " +
         (data == nullptr ? "" : std::string(data));
}

std::size_t Session::protocol::capacity() { return total; }

Session::context::context(Session::ptr session,
                          Session::protocol::ptr sendPacket)
    : session(session), packet(sendPacket) {}

Session::Session(io_context &ioContext)
    : socket(ioContext), recvBuffer(), closeEnable(false), sendMutex{},
      ioContext(ioContext) {}

Session::~Session() { Close(); }

void Session::SetSessionID(uint64_t id) { sessionId = id; }

void Session::Start() {
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
          spdlog::info("[{}] 的消息被丢弃，因其承载已超出最大承载量, "
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
        spdlog::info("[{}] 收到消息 服务ID: {}, 总长: {}",
                     GetEndpointToString(), (packet->id), packet->total);

        context::Ptr channel =
            std::make_shared<context>(shared_from_this(), packet);
        // 传送已序列化为本地字节序的消息包
        HandlePacket(channel);

        memset(recvBuffer, 0, PROTOCOL_HEADER_TOTAL);
      }
    }
  });
}

void Session::HandleError(error_code ec) {
  /*
    eof：表示对端关闭了连接
    connection_reset：表示对方异常断开连接
  */
  spdlog::info("[{}] 异常信息: {}", GetEndpointToString(), ec.message());

  Close();
  ClearSession();
}

void Session::Send(std::shared_ptr<protocol> packet) {
  std::lock_guard<std::mutex> lock(sendMutex);
  auto queueSize = sendQueue.size();
  try {
    if (queueSize > PROTOCOL_QUEUE_MAX_SIZE) {
      spdlog::info("[{}] 的消息被丢弃，因其承载已超出最大发送队列数量, "
                   "当前大小({}) > "
                   "最大发送队列数量({})",
                   GetEndpointToString(), queueSize, PROTOCOL_QUEUE_MAX_SIZE);
      return;
    }

    sendQueue.push(packet);
    if (queueSize > 0)
      return;

    auto &sendPacket = sendQueue.front();

    // 写入操作会发送完整的包数据
    spdlog::info("[{}] 发送消息 服务ID: {}, 总长: {}", GetEndpointToString(),
                 sendPacket->id, sendPacket->total);

    boost::asio::async_write(
        socket, boost::asio::buffer(sendPacket->data, sendPacket->total),
        std::bind(&Session::HandleWrite, this, std::placeholders::_1,
                  shared_from_this()));
  } catch (std::exception &e) {
    spdlog::info("[{}] 发送失败，异常信息： {}", GetEndpointToString(),
                 e.what());
  }
}

void Session::Close() {
  socket.close();
  closeEnable = true;
}

void Session::HandleWrite(const error_code &ec, Session::ptr sharedSelf) {
  std::lock_guard<std::mutex> lock(sendMutex);
  try {
    if (ec)
      return HandleError(ec);

    auto sendPacket = sendQueue.front();
    spdlog::info("[{}] 发送成功 服务ID: {}, 总长: {}", GetEndpointToString(),
                 (sendPacket->id), sendPacket->total);

    sendQueue.pop();

    if (!sendQueue.empty()) {
      auto &nextSendPacket = sendQueue.front();
      boost::asio::async_write(
          socket,
          boost::asio::buffer(nextSendPacket->data, nextSendPacket->total),
          std::bind(&Session::HandleWrite, this, std::placeholders::_1,
                    sharedSelf));
    }

  } catch (std::exception &e) {
    spdlog::info("[{}] 发送失败，异常信息： {}", GetEndpointToString(),
                 e.what());
  }
}

Session::tcp::socket &Session::GetSocket() { return socket; }

io_context &Session::GetIoc() { return ioContext; }

size_t Session::GetSessionID() { return sessionId; }

bool Session::IsConnected() { return !closeEnable; }

std::string Session::GetEndpointToString() {
  return socket.remote_endpoint().address().to_string() + ":" +
         std::to_string(socket.remote_endpoint().port());
}

Session::endpoint Session::GetEndpoint() { return socket.remote_endpoint(); }
}; // namespace wim