#include "ChatSession.h"
#include "ChatServer.h"
#include "Const.h"
#include "Service.h"

#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>

#include "Const.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <climits>

namespace wim {
Tlv::Tlv(unsigned int maxLen, unsigned int msgID) {
  int len = boost::asio::detail::socket_ops::network_to_host_long(maxLen) +
            PACKAGE_TOTAL_LEN;
  id = boost::asio::detail::socket_ops::network_to_host_long(msgID);

  data = new char[len];
  length = len;
  cur = PACKAGE_TOTAL_LEN;

  memcpy(data, &id, PACKAGE_ID_LEN);
  memcpy(data + PACKAGE_ID_LEN, &length, PACKAGE_DATA_SIZE_LEN);
}

Tlv::Tlv(unsigned int msgID, unsigned int maxLength, char *msg) {

  id = msgID;

  length = maxLength + PACKAGE_TOTAL_LEN;
  cur = length;
  data = new char[length];

  unsigned int tmpID =
      boost::asio::detail::socket_ops::host_to_network_long(id);
  unsigned int tmpLen =
      boost::asio::detail::socket_ops::host_to_network_long(maxLength);

  memcpy(data, &tmpID, PACKAGE_ID_LEN);
  memcpy(data + PACKAGE_ID_LEN, &tmpLen, PACKAGE_DATA_SIZE_LEN);
  memcpy(data + PACKAGE_ID_LEN + PACKAGE_DATA_SIZE_LEN, msg, maxLength);
}

std::string Tlv::getData() { return std::string(data, length); }

Tlv::~Tlv() {
  if (data) {
    delete[] data;
    data = nullptr;
  }
}

LogicProtocol::LogicProtocol(std::shared_ptr<ChatSession> chatSession,
                             std::shared_ptr<Tlv> package)
    : session(chatSession), recvPackage(package) {}
std::string LogicProtocol::getData() { return recvPackage->getData(); };

ChatSession::ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
                         size_t sessionID)
    : sock(ioContext), id(sessionID), chatServer(server), closeEnable(false),
      ioc(ioContext) {
  spdlog::info("ChatSession construct, sessionID is {}", sessionID);
}

ChatSession::~ChatSession() { spdlog::info("~ChatSession destruct"); }

void ChatSession::Start() { ReceiveHead(PACKAGE_TOTAL_LEN); }

void ChatSession::Send(char *msgData, unsigned int maxSize,
                       unsigned int msgID) {

  std::lock_guard<std::mutex> lock(_sendMutex);
  auto senderSize = sender.size();
  if (senderSize > MAX_SEND_QUEUE_LEN) {
    spdlog::error("session: {} send que fulled, size is {}", id,
                  MAX_SEND_QUEUE_LEN);
    return;
  }

  sender.push(std::make_shared<Tlv>(msgID, maxSize, msgData));
  if (senderSize > 0)
    return;

  // 按<id, total, data>协议回包
  auto &package = sender.front();

  // spdlog::info("Send package, sessionID is {}, packageID is {}, packageSize
  // is "
  //              "{}, packageData is {}",
  //              id, package->id, package->total, std::string(package->data));

  // 写入操作会发送完整的包数据
  boost::asio::async_write(
      sock, boost::asio::buffer(package->data, package->length),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                shared_from_this()));
}

void ChatSession::Send(std::string msgData, unsigned int msgID) {
  ChatSession::Send(msgData.data(), msgData.length(), msgID);
}

void ChatSession::Close() {
  sock.close();
  closeEnable = true;
}

void ChatSession::ReceiveBody(size_t size) {

  auto self = shared_from_this();
  asyncReadFull(size, [self, this, size](const boost::system::error_code &ec,
                                         std::size_t bytesTransfered) {
    try {
      if (ec) {
        spdlog::error(
            "[ChatSession::ReceivePackageBody] handle read failed, error is {}",
            ec.what());
        Close();
        chatServer->ClearSession(id);
        return;
      }

      if (bytesTransfered < size) {
        spdlog::warn("[ChatSession::ReceivePackageBody] read length not match, "
                     "read [{}] , total [{}]",
                     bytesTransfered, size);
        Close();
        chatServer->ClearSession(id);
        return;
      }

      spdlog::info("[ChatSession::ReceivePackageBody] receive packageBuf is {}",
                   packageBuf);

      memcpy(packageNode->data, packageBuf, bytesTransfered);
      packageNode->cur += bytesTransfered;
      packageNode->data[packageNode->length] = '\0';

      Service::GetInstance()->PushService(
          std::make_shared<ChatSession::RequestPackage>(shared_from_this(),
                                                        packageNode));
      ReceiveHead(PACKAGE_TOTAL_LEN);
    } catch (std::exception &e) {
      spdlog::error("[ChatSession::ReceivePackageBody] Exception code is {}",
                    e.what());
    }
  });
}

void ChatSession::ReceiveHead(size_t size) {
  auto self = shared_from_this();
  asyncReadFull(PACKAGE_TOTAL_LEN, [self,
                                    this](const boost::system::error_code &ec,
                                          std::size_t bytesTransfered) {
    try {
      if (ec == net::error::eof) {
        spdlog::info("[ChatSession::ReceivePackageHead] end of file, socket "
                     "close | oper-sessionID: {}",
                     id);
        Close();
        chatServer->ClearSession(id);
        return;
      }
      if (ec) {
        spdlog::error("[ChatSession::ReceivePackageHead] handle read failed, "
                      "error is {}",
                      ec.what());
        Close();
        chatServer->ClearSession(id);
        return;
      }
      if (bytesTransfered < PACKAGE_TOTAL_LEN) {
        spdlog::warn("[ChatSession::ReceivePackageHead] read length not match, "
                     "read [{}] , total [{}] | oper: close this session",
                     bytesTransfered, PACKAGE_TOTAL_LEN);
        Close();
        chatServer->ClearSession(id);
        return;
      }

      unsigned int serviceID = 0;
      unsigned int packageSize = 0;
      memcpy(&serviceID, packageBuf, PACKAGE_ID_LEN);
      memcpy(&packageSize, packageBuf + PACKAGE_ID_LEN, PACKAGE_DATA_SIZE_LEN);

      packageNode =
          std::make_shared<ChatSession::Protocol>(packageSize, serviceID);

      serviceID =
          boost::asio::detail::socket_ops::network_to_host_long(serviceID);

      packageSize =
          boost::asio::detail::socket_ops::network_to_host_long(packageSize);

      if (__IsSupportServiceID(static_cast<ServiceID>(serviceID)) == false) {
        spdlog::info(
            "[ChatSession::ReceivePackageHead] invalid serviceID-{} is  {}",
            serviceID,
            __MapSerivceIdToString(static_cast<ServiceID>(serviceID)));
        chatServer->ClearSession(id);
        return;
      } else {
        spdlog::info(
            "ChatSession::ReceivePackageHead] receive serviceID-{} is {} ",
            serviceID,
            __MapSerivceIdToString(static_cast<ServiceID>(serviceID)));
      }

      if (packageSize > PACKAGE_MAX_LENGTH || packageSize == 0) {
        spdlog::info("invalid data length is {}  ", packageSize);
        chatServer->ClearSession(id);
        return;
      }
      spdlog::info("receive serviceID is {}, packageLen is {} ", serviceID,
                   packageSize);

      ReceiveBody(packageSize);
    } catch (std::exception &e) {
      spdlog::error("Exception code is {}", e.what());
    }
  });
}

void ChatSession::HandleWrite(const boost::system::error_code &error,
                              std::shared_ptr<ChatSession> sharedSelf) {
  try {
    if (!error) {
      std::lock_guard<std::mutex> lock(_sendMutex);
      sender.pop();
      if (!sender.empty()) {
        auto &package = sender.front();
        boost::asio::async_write(
            sock, boost::asio::buffer(package->data, package->length),
            std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                      sharedSelf));
      }
    } else {
      spdlog::error(
          "[ChatSession::HandleWrite] handle write failed, error is {}",
          error.what());
      Close();
      chatServer->ClearSession(id);
    }
  } catch (std::exception &e) {
    spdlog::error("[ChatSession::HandleWrite] Exception code is {}", e.what());
  }
}

void ChatSession::asyncReadFull(
    std::size_t maxLength,
    std::function<void(const boost::system::error_code &, std::size_t)>
        handler) {
  memset(packageBuf, 0, PACKAGE_MAX_LENGTH);
  asyncRead(0, maxLength, handler);
}

void ChatSession::asyncRead(
    std::size_t readOffset, std::size_t total,
    std::function<void(const boost::system::error_code &, std::size_t)>
        handler) {
  auto self = shared_from_this();
  sock.async_read_some(
      boost::asio::buffer(packageBuf + readOffset, total - readOffset),
      [readOffset, total, handler, self](const boost::system::error_code &ec,
                                         std::size_t bytesTransfered) {
        if (ec) {
          handler(ec, readOffset + bytesTransfered);
          return;
        }

        if (readOffset + bytesTransfered >= total) {
          handler(ec, readOffset + bytesTransfered);
          return;
        }

        self->asyncRead(readOffset + bytesTransfered, total, handler);
      });
}

tcp::socket &ChatSession::GetSocket() { return sock; }

net::io_context &ChatSession::GetIoc() { return ioc; }

size_t ChatSession::GetSessionID() { return id; }

void ChatSession::ClearSession() { chatServer->ClearSession(id); }
}; // namespace wim