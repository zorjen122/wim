#include "MessageService.h"

#include "Const.h"
#include "DbGlobal.h"
#include "DeliveryService.h"
#include "Logger.h"
#include "Mysql.h"
#include "Redis.h"
#include "RequestContext.h"

#include <string>

namespace wim {

MessageService::MessageService(DeliveryService &deliveryService)
    : deliveryService(deliveryService) {}

TcpPacket MessageService::Ack(ChatSession::Ptr session, uint32_t msgID,
                              TcpPacket &request) {
  TcpPacket rsp;
  int64_t seq = request.seq();
  int64_t uid = request.uid();
  if (seq <= 0) {
    rsp.set_error(ErrorCodes::JsonParser);
    return rsp;
  }
  bool legacyReceipt =
      !request.has_receipt_type() ||
      request.receipt_type() == protocol::RECEIPT_TYPE_UNSPECIFIED;
  auto receiptType =
      legacyReceipt ? protocol::RECEIPT_TYPE_DELIVERED : request.receipt_type();
  // 通知类 transport ACK 只结束重传，不参与消息生命周期状态。
  if (receiptType == protocol::RECEIPT_TYPE_TRANSPORT) {
    deliveryService.Acknowledge(uid, seq);
    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }
  if (receiptType != protocol::RECEIPT_TYPE_DELIVERED &&
      receiptType != protocol::RECEIPT_TYPE_READ) {
    rsp.set_error(ErrorCodes::JsonParser);
    return rsp;
  }
  short status = receiptType == protocol::RECEIPT_TYPE_READ
                     ? db::Message::Status::READ
                     : db::Message::Status::DELIVERED;
  std::string readTime = receiptType == protocol::RECEIPT_TYPE_READ
                             ? getCurrentDateTime()
                             : std::string{};

  // 先完成 receiver 所有权校验和持久化，再取消重传定时器。
  int updated = db::MysqlDao::GetInstance()->updateMessageForReceiver(
      seq, uid, status, readTime);
  if (updated == 0 && legacyReceipt) {
    deliveryService.Acknowledge(uid, seq);
    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }
  if (updated <= 0) {
    LOG_WARN(wim::businessLogger,
             "ACK所有权校验失败或消息不存在, seq: {}, principal: {}", seq, uid);
    rsp.set_error(updated == -1 ? ErrorCodes::MysqlFailed
                                : ErrorCodes::MessageOwnershipInvalid);
    return rsp;
  }

  deliveryService.Acknowledge(uid, seq);
  rsp.set_error(ErrorCodes::Success);
  rsp.set_message_id(seq);
  rsp.set_message_state(receiptType == protocol::RECEIPT_TYPE_READ
                            ? protocol::MESSAGE_STATE_READ
                            : protocol::MESSAGE_STATE_DELIVERED);
  return rsp;
}

TcpPacket MessageService::SendFile(ChatSession::Ptr session, uint32_t msgID,
                                   TcpPacket &request) {
  /* 一面推送消息，一面存储消息，其中离线情况，
  message表中的content对文件消息而言无作用，因其存储在文件系统
  存储规则目前暂为：fileService/uid/[seq].txt
  */
  return {};
}

TcpPacket MessageService::SendGroupText(ChatSession::Ptr session,
                                        uint32_t msgID, TcpPacket &request) {
  /*
    1.检查群聊
    2.建立<seq, [member1, member2, ...]>映射
    3.按成员在线状态分发消息
  */
  return {};
}

TcpPacket MessageService::SendText(ChatSession::Ptr session, uint32_t msgID,
                                   TcpPacket &request) {
  TcpPacket rsp;

  int64_t clientSeq = request.seq();
  // uid 是入口注入的 canonical actor；from 仅用于兼容下行协议。
  int64_t from = request.uid();
  int64_t to = request.to();
  int64_t sessionHashKey = request.session_key();
  std::string data = request.data();

  // 发送者回应包中包含它的消息序列号，表示已接收并处理了请求
  rsp.set_seq(clientSeq);

  if (clientSeq <= 0 || to <= 0 || data.empty()) {
    rsp.set_error(ErrorCodes::JsonParser);
    rsp.set_message("seq, to and data are required");
    rsp.set_retryable(false);
    return rsp;
  }

  request.set_from(from);

  bool missMessageCache =
      db::RedisDao::GetInstance()->getUserMsgId(from, clientSeq);
  if (missMessageCache) {
    LOG_INFO(businessLogger, "重复消息, 客户端消息序列号为: {}", clientSeq);
    db::RedisDao::GetInstance()->expireUserMsgId(
        from, clientSeq, MESSAGE_CACHE_EXPIRE_TIME_SECONDS);
    rsp.set_error(ErrorCodes::RepeatMessage);
    rsp.set_message("重复消息");
    return rsp;
  }

  auto target = deliveryService.Locate(to);
  bool isLocalMachineOnline =
      target.location == DeliveryService::Location::Local;
  if (target.location == DeliveryService::Location::Remote) {
    LOG_INFO(businessLogger, "用户不在本地机器，转发消息到其他机器, 用户ID: {}",
             to);
    std::string machineId = target.machineId;
    std::string requestId;
    if (auto *context = RequestContextScope::Current()) {
      requestId = context->requestId;
    }

    auto deliveryResult = deliveryService.ForwardText(
        machineId, from, to, data, clientSeq, sessionHashKey, requestId);
    if (!deliveryResult.nodeFound) {
      LOG_WARN(businessLogger, "未找到目标IM机器的RPC连接, machineId: {}",
               machineId);
      rsp.set_status("wait");
      rsp.set_error(ErrorCodes::RPCFailed);
      rsp.set_message("目标IM机器不可达");
      rsp.set_retryable(true);
      return rsp;
    }

    LOG_INFO(businessLogger, "转发完成，目标机器: {}, 状态: {}", machineId,
             deliveryResult.status);
    rsp.set_status(deliveryResult.success ? "accepted" : "wait");
    if (!deliveryResult.success) {
      rsp.set_error(deliveryResult.error);
      rsp.set_retryable(isRetryableError(deliveryResult.error));
    } else {
      rsp.set_error(ErrorCodes::Success);
      rsp.set_message_id(deliveryResult.messageId);
      rsp.set_message_state(protocol::MESSAGE_STATE_ACCEPTED);
      rsp.set_retryable(false);
    }
    return rsp;
  }

  db::RedisDao::GetInstance()->setUserMsgId(from, clientSeq,
                                            MESSAGE_CACHE_EXPIRE_TIME_SECONDS);
  int64_t serverMsgSeq = db::RedisDao::GetInstance()->generateMsgId();
  const std::string sendTime = getCurrentDateTime();

  if (isLocalMachineOnline) {
    LOG_INFO(businessLogger, "用户本地在线，发送消息给目标用户，ID: {}", to);
    db::Message::Ptr message(new db::Message(
        serverMsgSeq, from, to, std::to_string(sessionHashKey),
        db::Message::Type::TEXT, data, db::Message::Status::WAIT, sendTime));
    int sqlStatus = db::MysqlDao::GetInstance()->insertMessage(message);
    if (sqlStatus == -1) {
      rsp.set_error(ErrorCodes::MysqlFailed);
      rsp.set_message("消息落库失败");
      rsp.set_retryable(true);
      return rsp;
    }

    rsp.set_status("accepted");
    rsp.set_error(ErrorCodes::Success);
    rsp.set_message_id(serverMsgSeq);
    rsp.set_message_state(protocol::MESSAGE_STATE_ACCEPTED);
    rsp.set_retryable(false);

    // 替换成服务端消息序列号，因需维护服务端道接收者的消息查收状态
    request.set_seq(serverMsgSeq);
    request.set_message_id(serverMsgSeq);
    request.set_message_state(protocol::MESSAGE_STATE_ACCEPTED);
    deliveryService.SendLocalReliable(
        to, serverMsgSeq, SerializeTcpPacket(request), ID_TEXT_SEND_REQ);
    return rsp;
  }

  LOG_INFO(businessLogger, "用户不在线，存储离线消息, 用户ID: {}", to);
  db::Message::Ptr message = nullptr;
  message.reset(new db::Message(
      serverMsgSeq, from, to, std::to_string(sessionHashKey),
      db::Message::Type::TEXT, data, db::Message::Status::WAIT, sendTime));

  int sqlStatus = db::MysqlDao::GetInstance()->insertMessage(message);
  rsp.set_status("accepted");
  if (sqlStatus != -1) {
    rsp.set_error(ErrorCodes::Success);
    rsp.set_message_id(serverMsgSeq);
    rsp.set_message_state(protocol::MESSAGE_STATE_ACCEPTED);
    rsp.set_retryable(false);
  } else {
    rsp.set_error(ErrorCodes::MysqlFailed);
    rsp.set_retryable(true);
  }
  return rsp;
}

TcpPacket MessageService::PullSessionMessages(ChatSession::Ptr session,
                                              uint32_t msgID,
                                              TcpPacket &request) {
  TcpPacket rsp;

  int64_t from = request.from();
  int64_t to = request.to();
  int64_t lastMsgId = request.last_msg_id();
  int limit = request.limit();

  auto messageList = db::MysqlDao::GetInstance()->getSessionMessage(
      from, to, lastMsgId, limit);
  if (messageList == nullptr) {
    LOG_INFO(wim::businessLogger,
             "消息表为空, from: {}, to: {}, lastMsgId: {}, limit: {}", from, to,
             lastMsgId, limit);

    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  rsp.set_uid(to);
  for (auto message : *messageList) {
    auto *item = rsp.add_message_list();
    item->set_message_id(message->messageId);
    item->set_type(message->type);
    item->set_content(message->content);
    item->set_status(message->status);
    item->set_send_date_time(message->sendDateTime);
    item->set_read_date_time(message->readDateTime);
  }
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

TcpPacket MessageService::PullMessages(ChatSession::Ptr session, uint32_t msgID,
                                       TcpPacket &request) {
  TcpPacket rsp;

  int64_t uid = request.uid();
  int64_t lastMsgId = request.last_msg_id();
  int limit = request.limit();

  rsp.set_uid(uid);

  auto messageList =
      db::MysqlDao::GetInstance()->getUserMessage(uid, lastMsgId, limit);
  if (messageList == nullptr) {
    LOG_INFO(wim::businessLogger,
             "消息表为空,  uid: {}, lastMsgId: {}, limit: {}", uid, lastMsgId,
             limit);

    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  for (auto message : *messageList) {
    auto *item = rsp.add_message_list();
    item->set_message_id(message->messageId);
    item->set_type(message->type);
    item->set_content(message->content);
    item->set_status(message->status);
    item->set_send_date_time(message->sendDateTime);
    item->set_read_date_time(message->readDateTime);
  }
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

}  // namespace wim
