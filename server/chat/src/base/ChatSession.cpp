#include "ChatSession.h"

#include "json/value.h"
#include <boost/asio/io_context.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>

#include "ChatServer.h"
#include "Const.h"
#include "ServiceSystem.h"

ChatSession::ChatSession(boost::asio::io_context &ioContext, ChatServer *server,
                         size_t id)
    : sock(ioContext), id(id), chatServer(server), closeEnable(false),
      ioc(ioContext) {
  spdlog::info("ChatSession construct, sessionID is {}", id);
}

ChatSession::~ChatSession() { spdlog::info("~ChatSession destruct"); }

void ChatSession::Start() { ReceiveHead(PACKAGE_TOTAL_LEN); }

// bool ChatSession::resetHandle() {
//   if (_hasPing) {
//     Json::Value rsp;
//     rsp["error"] = ErrorCodes::Success;
//     rsp["message"] = "pong";
//     std::string data = rsp.toStyledString();
//     Send(data, ID_PING_PONG_RSP);

//     _hasPing = false;

//     return true;
//   }
//   return false;
// }

// void ChatSession::tickleHandle() {
//   Close();
//   chatServer->ClearSession(sessionID);
// }

void ChatSession::Send(char *msgData, short maxSize, short msgID) {

  std::lock_guard<std::mutex> lock(_sendMutex);
  auto senderSize = sender.size();
  if (senderSize > MAX_SEND_QUEUE_LEN) {
    spdlog::error("session: {} send que fulled, size is {}", id,
                  MAX_SEND_QUEUE_LEN);
    return;
  }

  sender.push(std::make_shared<protocol::SendPackage>(msgData, maxSize, msgID));
  if (senderSize > 0)
    return;

  // 按<id, total, data>协议回包
  auto &package = sender.front();

  // 写入操作会发送完整的包数据
  boost::asio::async_write(
      sock, boost::asio::buffer(package->data, package->total),
      std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                shared_from_this()));
}

void ChatSession::Send(std::string msgData, short msgID) {
  ChatSession::Send(msgData.data(), msgData.length(), msgID);
}

void ChatSession::Close() {
  sock.close();
  closeEnable = true;
}

void ChatSession::ReceiveBody(size_t size) {
  ::memset(packageBuf, 0, PACKAGE_MAX_LENGTH);

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

      memcpy(packageNode->data, packageBuf, bytesTransfered);
      packageNode->cur += bytesTransfered;
      packageNode->data[packageNode->total] = '\0';

      spdlog::info("[ChatSession::ReceivePackageBody] receive data is {}",
                   packageNode->data);

      ServiceSystem::GetInstance()->PushService(
          std::make_shared<protocol::LogicPackage>(shared_from_this(),
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
      if (ec) {
        spdlog::error(
            "[ChatSession::ReceivePackageHead] handle read failed, error is {}",
            ec.what());
        Close();
        chatServer->ClearSession(id);
        return;
      }

      if (bytesTransfered < PACKAGE_TOTAL_LEN) {
        spdlog::warn("[ChatSession::ReceivePackageHead] read length not match, "
                     "read [{}] , total [{}]",
                     bytesTransfered, PACKAGE_TOTAL_LEN);
        Close();
        chatServer->ClearSession(id);
        return;
      }

      unsigned short packageSize = 0, serviceID = 0;
      memcpy(&serviceID, packageBuf, PACKAGE_ID_LEN);
      serviceID =
          boost::asio::detail::socket_ops::network_to_host_short(serviceID);

      if (__IsSupportServiceID(static_cast<ServiceID>(serviceID)) == false) {
        spdlog::info(
            "[ChatSession::ReceivePackageHead] invalid packageID is  {}",
            __MapSerivceIdToString(static_cast<ServiceID>(serviceID)));
        chatServer->ClearSession(id);
        return;
      } else {
        spdlog::info(
            "ChatSession::ReceivePackageHead] receive packageID is {} ",
            __MapSerivceIdToString(static_cast<ServiceID>(serviceID)));
      }

      memcpy(&packageSize, packageBuf + PACKAGE_ID_LEN, PACKAGE_DATA_LEN);
      packageSize =
          boost::asio::detail::socket_ops::network_to_host_short(packageSize);

      if (packageSize > PACKAGE_MAX_LENGTH) {
        spdlog::info("invalid data length is {}  ", packageSize);
        chatServer->ClearSession(id);
        return;
      }
      spdlog::info("receive packageID is {}, packageLen is {} ");

      packageNode =
          std::make_shared<protocol::RecvPackage>(packageSize, serviceID);
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
            sock, boost::asio::buffer(package->data, package->total),
            std::bind(&ChatSession::HandleWrite, this, std::placeholders::_1,
                      sharedSelf));
      }
    } else {
      spdlog::error("handle write failed, error is {}", error.what());
      Close();
      chatServer->ClearSession(id);
    }
  } catch (std::exception &e) {
    spdlog::error("Exception code is {}", e.what());
  }
}

void ChatSession::asyncReadFull(
    std::size_t maxLength,
    std::function<void(const boost::system::error_code &, std::size_t)>
        handler) {
  asyncReadLen(0, maxLength, handler);
}

void ChatSession::asyncReadLen(
    std::size_t read_len, std::size_t total_len,
    std::function<void(const boost::system::error_code &, std::size_t)>
        handler) {
  auto self = shared_from_this();
  sock.async_read_some(
      boost::asio::buffer(packageBuf + read_len, total_len - read_len),
      [read_len, total_len, handler, self](const boost::system::error_code &ec,
                                           std::size_t bytesTransfered) {
        if (ec) {
          handler(ec, read_len + bytesTransfered);
          return;
        }

        if (read_len + bytesTransfered >= total_len) {
          handler(ec, read_len + bytesTransfered);
          return;
        }

        self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
      });
}

tcp::socket &ChatSession::GetSocket() { return sock; }

net::io_context &ChatSession::GetIoc() { return ioc; }