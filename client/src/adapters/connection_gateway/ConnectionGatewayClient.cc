#include "adapters/connection_gateway/ConnectionGatewayClient.h"

#include "adapters/connection_gateway/ClientProtocol.h"
#include "tcp_message.pb.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace wim::client {
namespace {

constexpr int kHeartbeatIntervalMilliseconds = 20'000;
constexpr int kReconnectBaseMilliseconds = 200;
constexpr int kReconnectMaximumMilliseconds = 10'000;
constexpr int kServerRequestBudgetMilliseconds = 3'000;

QByteArray Serialize(const wim::protocol::Packet &packet) {
  std::string bytes;
  if (!packet.SerializeToString(&bytes)) {
    return {};
  }
  return QByteArray::fromStdString(bytes);
}

bool Parse(const QByteArray &payload, wim::protocol::Packet *packet) {
  return packet != nullptr &&
         packet->ParseFromArray(payload.constData(), payload.size());
}

QString NewRequestId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}  // namespace

ConnectionGatewayClient::ConnectionGatewayClient(QObject *parent)
    : QObject(parent),
      next_legacy_sequence_(
          std::max<qint64>(1, QDateTime::currentMSecsSinceEpoch())) {
  qRegisterMetaType<State>();
  heartbeat_timer_.setInterval(kHeartbeatIntervalMilliseconds);
  reconnect_timer_.setSingleShot(true);

  connect(&socket_, &QTcpSocket::connected, this, [this] {
    reconnect_attempt_ = 0;
    SetState(State::Authenticating);
    wim::protocol::Packet login;
    login.set_uid(session_.uid);
    login.set_auth_token(session_.token.toStdString());
    SendPacket(protocol::LoginRequest, login);
  });
  connect(&socket_, &QTcpSocket::readyRead, this, [this] {
    const auto frames = codec_.Feed(socket_.readAll());
    if (codec_.HasError()) {
      emit ProtocolError(codec_.ErrorString());
      socket_.abort();
      return;
    }
    for (const auto &frame : frames) {
      HandleFrame(frame);
    }
  });
  connect(&socket_, &QTcpSocket::disconnected, this, [this] {
    heartbeat_timer_.stop();
    codec_.Reset();
    FailAllPending(protocol::DependencyUnavailable,
                   tr("Connection Gateway 连接已断开"), true);
    if (desired_open_ && state_ != State::Closing) {
      ScheduleReconnect();
    } else {
      SetState(State::Disconnected);
    }
  });
  connect(&socket_, &QTcpSocket::errorOccurred, this,
          [this](QAbstractSocket::SocketError) {
            if (state_ == State::Connecting || state_ == State::Reconnecting) {
              emit ProtocolError(socket_.errorString());
            }
          });
  connect(&heartbeat_timer_, &QTimer::timeout, this, [this] {
    if (!IsReady()) {
      return;
    }
    wim::protocol::Packet ping;
    SendPacket(protocol::PingRequest, ping);
  });
  connect(&reconnect_timer_, &QTimer::timeout, this,
          &ConnectionGatewayClient::StartSocket);
}

ConnectionGatewayClient::State ConnectionGatewayClient::CurrentState() const {
  return state_;
}

bool ConnectionGatewayClient::IsReady() const {
  return state_ == State::Ready;
}

std::int64_t ConnectionGatewayClient::UserId() const {
  return session_.uid;
}

void ConnectionGatewayClient::Open(const GateSession &session) {
  desired_open_ = false;
  if (socket_.state() != QAbstractSocket::UnconnectedState) {
    socket_.abort();
  }
  session_ = session;
  token_expires_at_milliseconds_ =
      QDateTime::currentMSecsSinceEpoch() +
      std::max<qint64>(0, session.tokenExpiresInSeconds) * 1000;
  reconnect_attempt_ = 0;
  reconnect_timer_.stop();
  heartbeat_timer_.stop();
  desired_open_ = true;
  StartSocket();
}

void ConnectionGatewayClient::Close() {
  desired_open_ = false;
  reconnect_timer_.stop();
  heartbeat_timer_.stop();
  if (socket_.state() == QAbstractSocket::UnconnectedState) {
    SetState(State::Disconnected);
    return;
  }
  SetState(State::Closing);
  wim::protocol::Packet quit;
  SendPacket(protocol::QuitRequest, quit);
  QTimer::singleShot(1000, this, [this] {
    if (state_ == State::Closing) {
      socket_.disconnectFromHost();
    }
  });
}

QString ConnectionGatewayClient::PullFriendList() {
  return QueueRequest(protocol::PullFriendListRequest, {});
}

QString ConnectionGatewayClient::PullFriendApplications() {
  return QueueRequest(protocol::PullFriendApplyListRequest, {});
}

QString ConnectionGatewayClient::PullConversationMessages(
    std::int64_t conversationId, std::int64_t afterSequence, int limit) {
  wim::protocol::Packet packet;
  packet.set_conversation_id(conversationId);
  packet.set_after_seq(std::max<std::int64_t>(0, afterSequence));
  packet.set_limit(std::clamp(limit, 1, 200));
  return QueueRequest(protocol::PullSessionMessagesRequest, std::move(packet));
}

QString ConnectionGatewayClient::PullAllMessages(std::int64_t lastMessageId,
                                                 int limit) {
  wim::protocol::Packet packet;
  packet.set_last_msg_id(std::max<std::int64_t>(0, lastMessageId));
  packet.set_limit(std::clamp(limit, 1, 200));
  return QueueRequest(protocol::PullMessagesRequest, std::move(packet));
}

QString ConnectionGatewayClient::SendFriendRequest(std::int64_t recipientUid,
                                                   const QString &message) {
  wim::protocol::Packet packet;
  packet.set_to(recipientUid);
  packet.set_request_message(message.toStdString());
  return QueueRequest(protocol::AddFriendRequest, std::move(packet));
}

QString ConnectionGatewayClient::ReplyFriendRequest(std::int64_t recipientUid,
                                                    bool accept,
                                                    const QString &message) {
  wim::protocol::Packet packet;
  packet.set_to(recipientUid);
  packet.set_accept(accept);
  packet.set_reply_message(message.toStdString());
  return QueueRequest(protocol::ReplyFriendRequest, std::move(packet));
}

QString ConnectionGatewayClient::SendText(std::int64_t recipientUid,
                                          const QByteArray &utf8Text,
                                          const QString &clientMessageId,
                                          std::int64_t conversationId) {
  wim::protocol::Packet packet;
  packet.set_seq(NextLegacySequence());
  packet.set_to(recipientUid);
  packet.set_data(utf8Text.constData(), utf8Text.size());
  packet.set_client_message_id(clientMessageId.toStdString());
  if (conversationId > 0) {
    packet.set_conversation_id(conversationId);
    packet.set_session_key(conversationId);
  }
  return QueueRequest(protocol::SendTextRequest, std::move(packet));
}

QString ConnectionGatewayClient::UploadFile(std::int64_t clientSequence,
                                            const QString &fileName,
                                            const QString &fileType,
                                            const QByteArray &content) {
  wim::protocol::Packet packet;
  packet.set_seq(clientSequence);
  packet.set_file_name(fileName.toStdString());
  packet.set_file_type(fileType.toStdString());
  packet.set_data(content.constData(), content.size());
  return QueueRequest(protocol::UploadFileRequest, std::move(packet), 10'000);
}

QString ConnectionGatewayClient::CreateGroup(const QString &groupName) {
  wim::protocol::Packet packet;
  packet.set_group_name(groupName.toStdString());
  return QueueRequest(protocol::CreateGroupRequest, std::move(packet));
}

QString ConnectionGatewayClient::RequestJoinGroup(std::int64_t groupId,
                                                  const QString &message) {
  wim::protocol::Packet packet;
  packet.set_gid(groupId);
  packet.set_request_message(message.toStdString());
  return QueueRequest(protocol::JoinGroupRequest, std::move(packet));
}

QString ConnectionGatewayClient::ReplyJoinGroup(std::int64_t groupId,
                                                std::int64_t requestorUid,
                                                bool accept) {
  wim::protocol::Packet packet;
  packet.set_gid(groupId);
  packet.set_requestor_uid(requestorUid);
  packet.set_accept(accept);
  return QueueRequest(protocol::ReplyJoinGroupRequest, std::move(packet));
}

QString ConnectionGatewayClient::SendGroupText(std::int64_t groupId,
                                               const QByteArray &utf8Text,
                                               const QString &clientMessageId,
                                               std::int64_t conversationId) {
  wim::protocol::Packet packet;
  packet.set_seq(NextLegacySequence());
  packet.set_gid(groupId);
  packet.set_data(utf8Text.constData(), utf8Text.size());
  packet.set_client_message_id(clientMessageId.toStdString());
  if (conversationId > 0) {
    packet.set_conversation_id(conversationId);
    packet.set_session_key(conversationId);
  }
  return QueueRequest(protocol::SendGroupTextRequest, std::move(packet));
}

void ConnectionGatewayClient::AcknowledgeTransport(std::int64_t messageId) {
  SendReceipt(messageId, wim::protocol::RECEIPT_TYPE_TRANSPORT, 0, 0);
}

void ConnectionGatewayClient::AcknowledgeDelivered(
    std::int64_t messageId, std::int64_t conversationId,
    std::int64_t conversationSequence) {
  SendReceipt(messageId, wim::protocol::RECEIPT_TYPE_DELIVERED, conversationId,
              conversationSequence);
}

void ConnectionGatewayClient::AcknowledgeRead(
    std::int64_t messageId, std::int64_t conversationId,
    std::int64_t conversationSequence) {
  SendReceipt(messageId, wim::protocol::RECEIPT_TYPE_READ, conversationId,
              conversationSequence);
}

QString ConnectionGatewayClient::QueueRequest(quint32 serviceId,
                                              wim::protocol::Packet packet,
                                              int timeoutMilliseconds) {
  const QString requestId = NewRequestId();
  if (!IsReady()) {
    emit RequestFailed(requestId, serviceId, protocol::DependencyUnavailable,
                       tr("Connection Gateway 尚未就绪"), false);
    return requestId;
  }

  packet.set_request_id(requestId.toStdString());
  packet.set_request_timeout_ms(kServerRequestBudgetMilliseconds);
  const QByteArray payload = Serialize(packet);
  if (payload.isEmpty() && packet.ByteSizeLong() != 0) {
    emit RequestFailed(requestId, serviceId, protocol::JsonParser,
                       tr("无法序列化请求"), false);
    return requestId;
  }

  const quint32 responseServiceId = protocol::ResponseFor(serviceId);
  auto &queue = pending_by_response_[responseServiceId];
  queue.enqueue(PendingRequest{
      .requestId = requestId,
      .requestServiceId = serviceId,
      .responseServiceId = responseServiceId,
      .payload = payload,
      .timeoutMilliseconds = timeoutMilliseconds,
  });
  if (queue.size() == 1) {
    SendFront(responseServiceId);
  }
  return requestId;
}

void ConnectionGatewayClient::SendFront(quint32 responseServiceId) {
  auto found = pending_by_response_.find(responseServiceId);
  if (found == pending_by_response_.end() || found->isEmpty() || !IsReady()) {
    return;
  }
  const PendingRequest &pending = found->head();
  socket_.write(
      TcpFrameCodec::Encode(pending.requestServiceId, pending.payload));

  QTimer *timer = request_timers_.value(responseServiceId);
  if (timer == nullptr) {
    timer = new QTimer(this);
    timer->setSingleShot(true);
    request_timers_.insert(responseServiceId, timer);
    connect(timer, &QTimer::timeout, this, [this, responseServiceId] {
      auto current = pending_by_response_.find(responseServiceId);
      if (current == pending_by_response_.end() || current->isEmpty()) {
        return;
      }
      const PendingRequest timedOut = current->dequeue();
      emit RequestFailed(timedOut.requestId, timedOut.requestServiceId,
                         protocol::DeadlineExceeded, tr("请求等待响应超时"),
                         true);
      if (current->isEmpty()) {
        pending_by_response_.erase(current);
      } else {
        SendFront(responseServiceId);
      }
    });
  }
  timer->start(pending.timeoutMilliseconds);
}

void ConnectionGatewayClient::CompleteFront(quint32 responseServiceId,
                                            const QByteArray &payload) {
  auto found = pending_by_response_.find(responseServiceId);
  if (found == pending_by_response_.end() || found->isEmpty()) {
    emit ProtocolError(
        tr("收到没有在途请求的响应 service=%1").arg(responseServiceId));
    return;
  }
  if (auto *timer = request_timers_.value(responseServiceId)) {
    timer->stop();
  }
  const PendingRequest completed = found->dequeue();
  emit ResponseReceived(completed.requestId, responseServiceId, payload);
  if (found->isEmpty()) {
    pending_by_response_.erase(found);
  } else {
    SendFront(responseServiceId);
  }
}

void ConnectionGatewayClient::FailAllPending(int errorCode,
                                             const QString &message,
                                             bool outcomeUnknown) {
  for (auto *timer : std::as_const(request_timers_)) {
    timer->stop();
  }
  for (auto iterator = pending_by_response_.begin();
       iterator != pending_by_response_.end(); ++iterator) {
    while (!iterator->isEmpty()) {
      const PendingRequest pending = iterator->dequeue();
      emit RequestFailed(pending.requestId, pending.requestServiceId, errorCode,
                         message, outcomeUnknown);
    }
  }
  pending_by_response_.clear();
}

void ConnectionGatewayClient::SendReceipt(std::int64_t messageId,
                                          int receiptType,
                                          std::int64_t conversationId,
                                          std::int64_t conversationSequence) {
  if (!IsReady() || messageId <= 0) {
    return;
  }
  wim::protocol::Packet receipt;
  receipt.set_seq(messageId);
  receipt.set_receipt_type(
      static_cast<wim::protocol::ReceiptType>(receiptType));
  if (conversationId > 0 && conversationSequence > 0) {
    receipt.set_conversation_id(conversationId);
    receipt.set_conversation_seq(conversationSequence);
  }
  SendPacket(protocol::Ack, receipt);
}

void ConnectionGatewayClient::SendPacket(quint32 serviceId,
                                         const wim::protocol::Packet &packet) {
  const QByteArray payload = Serialize(packet);
  if (payload.isEmpty() && packet.ByteSizeLong() != 0) {
    emit ProtocolError(tr("无法序列化 service=%1").arg(serviceId));
    return;
  }
  socket_.write(TcpFrameCodec::Encode(serviceId, payload));
}

void ConnectionGatewayClient::HandleFrame(const TcpFrame &frame) {
  if (frame.serviceId == protocol::LoginResponse) {
    HandleLoginResponse(frame.payload);
    return;
  }
  if (frame.serviceId == protocol::PingResponse) {
    return;
  }
  if (frame.serviceId == protocol::QuitResponse) {
    desired_open_ = false;
    socket_.disconnectFromHost();
    return;
  }

  wim::protocol::Packet packet;
  if (!Parse(frame.payload, &packet)) {
    emit ProtocolError(
        tr("无法解析 service=%1 的 Protobuf Packet").arg(frame.serviceId));
    socket_.abort();
    return;
  }
  if ((frame.serviceId & 1U) == 0U) {
    CompleteFront(frame.serviceId, frame.payload);
  } else {
    emit PushReceived(frame.serviceId, frame.payload);
  }
}

void ConnectionGatewayClient::HandleLoginResponse(const QByteArray &payload) {
  wim::protocol::Packet response;
  if (!Parse(payload, &response)) {
    emit ProtocolError(tr("无法解析 Gateway 登录响应"));
    desired_open_ = false;
    socket_.abort();
    return;
  }
  const int errorCode = response.has_error() ? response.error() : 0;
  if (errorCode != protocol::Success) {
    desired_open_ = false;
    if (errorCode == protocol::TokenInvalid ||
        errorCode == protocol::AuthenticationRequired) {
      emit CredentialsExpired();
    } else {
      emit ProtocolError(tr("Gateway 登录失败（%1）").arg(errorCode));
    }
    socket_.disconnectFromHost();
    return;
  }

  SetState(State::Ready);
  heartbeat_timer_.start();
  emit Authenticated();
}

void ConnectionGatewayClient::ScheduleReconnect() {
  if (token_expires_at_milliseconds_ > 0 &&
      QDateTime::currentMSecsSinceEpoch() >= token_expires_at_milliseconds_) {
    desired_open_ = false;
    SetState(State::Disconnected);
    emit CredentialsExpired();
    return;
  }

  SetState(State::Reconnecting);
  const int exponent = std::min(reconnect_attempt_, 6);
  const int base = std::min(kReconnectMaximumMilliseconds,
                            kReconnectBaseMilliseconds * (1 << exponent));
  const int jitter = QRandomGenerator::global()->bounded(base / 4 + 1);
  ++reconnect_attempt_;
  reconnect_timer_.start(
      std::min(kReconnectMaximumMilliseconds, base + jitter));
}

void ConnectionGatewayClient::StartSocket() {
  if (!desired_open_) {
    return;
  }
  codec_.Reset();
  SetState(reconnect_attempt_ == 0 ? State::Connecting : State::Reconnecting);
  socket_.connectToHost(session_.gatewayHost, session_.gatewayPort);
}

void ConnectionGatewayClient::SetState(State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  emit StateChanged(state_);
}

std::int64_t ConnectionGatewayClient::NextLegacySequence() {
  return next_legacy_sequence_++;
}

}  // namespace wim::client
