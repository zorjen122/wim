#include "chatSession.h"
#include "Const.h"
#include "Logger.h"
#include "chat.h"

#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <spdlog/spdlog.h>
#include <string>

#include "Const.h"
#include "client.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <climits>

namespace wim {
Tlv::Tlv(uint32_t maxLen, uint32_t msgID) {

  uint32_t tmp = ntohl(maxLen);
  id = ntohl(msgID);
  length = tmp + PROTOCOL_HEADER_TOTAL;

  data = new char[tmp + 1];
}

Tlv::Tlv(uint32_t msgID, uint32_t maxLength, char *msg) {

  id = msgID;

  length = maxLength + PROTOCOL_HEADER_TOTAL;
  data = new char[length];

  uint32_t tmpID = htonl(id);
  uint32_t tmpLen = htonl(maxLength);

  memcpy(data, &tmpID, PROTOCOL_ID_LEN);
  memcpy(data + PROTOCOL_ID_LEN, &tmpLen, PROTOCOL_DATA_SIZE_LEN);
  memcpy(data + PROTOCOL_ID_LEN + PROTOCOL_DATA_SIZE_LEN, msg, maxLength);
}

uint32_t Tlv::getDataSize() { return length - PROTOCOL_HEADER_TOTAL; }
uint32_t Tlv::getTotal() { return length; }

std::string Tlv::getDataString() { return std::string(data, length); }
char *Tlv::getData() { return data; }

Tlv::~Tlv() {
  if (data) {
    delete[] data;
    data = nullptr;
  }
}

ChatSession::ChatSession(net::io_context &iocontext, Endpoint endpoint)
    : parseState(WAIT_HEADER), iocontext(iocontext) {
  chat.reset(new tcp::socket(iocontext));

  boost::system::error_code ec;

  LOG_INFO(wim::businessLogger, "chat connect to {}:{}", endpoint.ip,
           endpoint.port);
  net::ip::tcp::endpoint ep(net::ip::address::from_string(endpoint.ip),
                            std::stoi(endpoint.port));

  ec = chat->connect(ep, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  recvStreamBuffer.prepare(PROTOCOL_DATA_MTU);
}
ChatSession::~ChatSession() {
  spdlog::info("ChatSession::~ChatSession");
  Close();
}

bool ChatSession::isConnected() { return chat->is_open(); }

void ChatSession::Start() {
  switch (parseState) {
  case WAIT_HEADER: {
    boost::asio::async_read(
        *chat, recvStreamBuffer,
        boost::asio::transfer_exactly(PROTOCOL_HEADER_TOTAL),
        [this, self = shared_from_this()](net::error_code ec, size_t) {
          try {
            if (ec)
              return HandleError(ec);

            uint32_t currentMsgID = 0;
            uint32_t expectedBodyLen = 0;
            std::istream is(&recvStreamBuffer);
            is.read(reinterpret_cast<char *>(&currentMsgID), sizeof(uint32_t));
            is.read(reinterpret_cast<char *>(&expectedBodyLen),
                    sizeof(uint32_t));

            recvProtocolData = std::make_shared<ChatSession::Protocol>(
                expectedBodyLen, currentMsgID);

            // 验证长度有效性
            if (recvProtocolData->getDataSize() > PROTOCOL_RECV_MSS) {
              LOG_WARN(netLogger,
                       "invalid data getDataSize() is {} > PROTOCOL_RECV_MSS",
                       recvProtocolData->getDataSize());
              recvStreamBuffer.consume(PROTOCOL_HEADER_TOTAL);
              parseState = WAIT_HEADER;
              return;
            }
            recvStreamBuffer.consume(PROTOCOL_HEADER_TOTAL);
            parseState = ParseState::WAIT_BODY;

            LOG_INFO(netLogger, "recvProtocolData->getDataSize() is {}",
                     recvProtocolData->getDataSize());
            Start();
          } catch (std::exception &e) {
            LOG_ERROR(netLogger, "[ChatSession::Start] Exception code is {}",
                      e.what());
          }
        });
    break;
  }
  case WAIT_BODY: {
    try {
      boost::asio::async_read(
          *chat, recvStreamBuffer,
          boost::asio::transfer_exactly(recvProtocolData->getDataSize()),
          [this, self = shared_from_this()](net::error_code ec, size_t byte) {
            if (ec)
              return HandleError(ec);

            if (byte != recvProtocolData->getDataSize()) {
              LOG_ERROR(netLogger,
                        "boost::asio::async_read完整读取出现异常, "
                        "数据长度不匹配, 期望长度:{}, 实际长度:{}",
                        byte, recvProtocolData->getDataSize());
              recvStreamBuffer.consume(byte);
              parseState = ParseState::WAIT_HEADER;
              Start();
            }

            memcpy(recvProtocolData->data, recvStreamBuffer.data().data(),
                   byte);
            Chat::GetInstance()->handleRun(recvProtocolData);
            recvStreamBuffer.consume(byte);

            // 回到头解析状态
            parseState = ParseState::WAIT_HEADER;
            Start();
          });
    } catch (std::exception &e) {
      LOG_ERROR(netLogger, "[ChatSession::Start] Exception code is {}",
                e.what());
      parseState = ParseState::WAIT_HEADER;
    }
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
    LOG_DEBUG(netLogger, "end of file, chat close ");
  } else if (ec == net::error::connection_reset) {
    LOG_DEBUG(netLogger, "connection reset, chat close ");
  } else {
    LOG_DEBUG(netLogger, "handle error, error is {} ");
  }

  Close();
}

void ChatSession::Send(char *msgData, uint32_t maxSize, uint32_t msgID) {
  LOG_INFO(netLogger, "预发送大小为：{}, 请求服务ID：{}", maxSize,
           getServiceIdString(msgID));
  sendProtocolData.reset(new Tlv(msgID, maxSize, msgData));

  // 写入操作会发送完整的包数据
  boost::asio::async_write(
      *chat,
      boost::asio::buffer(sendProtocolData->data, sendProtocolData->length),
      [this](net::error_code ec, size_t) {
        if (ec)
          HandleError(ec);
        sendProtocolData.reset();
      });
}

void ChatSession::Send(std::string msgData, uint32_t msgID) {
  ChatSession::Send(msgData.data(), msgData.length(), msgID);
}

void ChatSession::Close() {
  if (chat && chat->is_open()) {
    chat->close(); // 显式关闭 socket
    closeEnable = true;
  }
}

}; // namespace wim