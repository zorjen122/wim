#include "app/AppController.h"

#include "adapters/connection_gateway/ClientProtocol.h"
#include "adapters/fake/FakeScenarioRepository.h"
#include "adapters/sqlite/SqliteClientRepository.h"
#include "tcp_message.pb.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace wim::client {
namespace {

bool ParsePacket(const QByteArray &payload, wim::protocol::Packet *packet) {
  return packet != nullptr &&
         packet->ParseFromArray(payload.constData(), payload.size());
}

QString DirectConversationId(std::int64_t peerUid) {
  return QStringLiteral("direct:%1").arg(peerUid);
}

std::optional<std::int64_t> DirectPeerUid(const QString &conversationId) {
  constexpr auto prefix = "direct:";
  if (!conversationId.startsWith(QLatin1String(prefix))) {
    return std::nullopt;
  }
  bool valid = false;
  const auto uid = conversationId.mid(sizeof(prefix) - 1).toLongLong(&valid);
  return valid && uid > 0 ? std::optional<std::int64_t>(uid) : std::nullopt;
}

QString DisplayTimestamp(const std::string &serverTimestamp) {
  const QString value = QString::fromStdString(serverTimestamp);
  if (value.isEmpty()) {
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
  }
  return value.size() >= 5 ? value.right(8).left(5) : value;
}

}  // namespace

AppController::AppController(QObject *parent)
    : AppController(CreatePlatformServices(), parent) {}

AppController::AppController(
    std::unique_ptr<IPlatformServices> platformServices, QObject *parent)
    : QObject(parent), platform_services_(std::move(platformServices)) {
  if (platform_services_ == nullptr) {
    platform_services_ = CreatePlatformServices();
  }
  requested_window_width_ =
      ArgumentValue(QStringLiteral("width"), QStringLiteral("1280")).toInt();
  requested_window_height_ =
      ArgumentValue(QStringLiteral("height"), QStringLiteral("800")).toInt();
  dark_theme_requested_ =
      ArgumentValue(QStringLiteral("theme"), QStringLiteral("light")) ==
      QStringLiteral("dark");
  compact_conversation_requested_ = QCoreApplication::arguments().contains(
      QStringLiteral("--open-conversation"));
  const QString gateUrl = ArgumentValue(QStringLiteral("gate-url"), QString{});
  network_enabled_ = !gateUrl.isEmpty();
  const QString repositoryKind = ArgumentValue(
      QStringLiteral("repository"),
      network_enabled_ ? QStringLiteral("sqlite") : QStringLiteral("fake"));
  if (repositoryKind == QStringLiteral("sqlite")) {
    QString databasePath = ArgumentValue(QStringLiteral("database"), QString{});
    if (databasePath.isEmpty()) {
      QString account =
          ArgumentValue(QStringLiteral("account"), QStringLiteral("demo"));
      account.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")),
                      QStringLiteral("_"));
      QString dataDirectory =
          ArgumentValue(QStringLiteral("data-dir"), QString{});
      if (dataDirectory.isEmpty()) {
        dataDirectory = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
      }
      databasePath =
          QDir(dataDirectory)
              .filePath(QStringLiteral("accounts/%1.sqlite").arg(account));
    }
    repository_ = std::make_unique<SqliteClientRepository>(
        std::move(databasePath), !network_enabled_);
  } else {
    repository_ = std::make_unique<FakeScenarioRepository>();
  }

  LoadScenario(
      ArgumentValue(QStringLiteral("scenario"), QStringLiteral("normal")));
  if (network_enabled_) {
    ConfigureNetwork(gateUrl);
  }
  setCurrentSection(
      ArgumentValue(QStringLiteral("section"), QStringLiteral("chats")));
}

AppController::~AppController() = default;

ConversationListModel *AppController::conversations() {
  return &conversations_;
}

MessageListModel *AppController::messages() {
  return &messages_;
}

ContactListModel *AppController::contacts() {
  return &contacts_;
}

RequestListModel *AppController::requests() {
  return &requests_;
}

QStringList AppController::scenarios() const {
  return repository_->ScenarioNames();
}

QString AppController::scenarioName() const {
  return snapshot_.scenarioName;
}

QString AppController::connectionStatus() const {
  return snapshot_.connectionStatus;
}

int AppController::selectedConversationIndex() const {
  return selected_conversation_index_;
}

QString AppController::selectedConversationTitle() const {
  const auto *record = conversations_.RecordAt(selected_conversation_index_);
  return record == nullptr ? QString{} : record->title;
}

int AppController::selectedConversationUnreadCount() const {
  const auto *record = conversations_.RecordAt(selected_conversation_index_);
  return record == nullptr ? 0 : record->unreadCount;
}

QString AppController::draftText() const {
  return drafts_.value(CurrentStateKey());
}

QString AppController::currentSection() const {
  return current_section_;
}

int AppController::selectedContactIndex() const {
  return selected_contact_index_;
}

QString AppController::selectedContactName() const {
  const auto *record = contacts_.RecordAt(selected_contact_index_);
  return record == nullptr ? QString{} : record->displayName;
}

QString AppController::selectedContactStatus() const {
  const auto *record = contacts_.RecordAt(selected_contact_index_);
  return record == nullptr ? QString{} : record->statusText;
}

bool AppController::authRequired() const {
  return !authentication_completed_ &&
         (network_enabled_ ||
          snapshot_.connectionStatus == QStringLiteral("auth-expired"));
}

bool AppController::networkEnabled() const {
  return network_enabled_;
}

bool AppController::authenticationBusy() const {
  return authentication_busy_;
}

QString AppController::authenticationError() const {
  return authentication_error_;
}

QString AppController::serviceActionStatus() const {
  return service_action_status_;
}

int AppController::requestedWindowWidth() const {
  return requested_window_width_;
}

int AppController::requestedWindowHeight() const {
  return requested_window_height_;
}

bool AppController::darkThemeRequested() const {
  return dark_theme_requested_;
}

bool AppController::compactConversationRequested() const {
  return compact_conversation_requested_;
}

QString AppController::repositoryKind() const {
  return repository_->RepositoryKind();
}

QString AppController::platformName() const {
  return platform_services_->PlatformName();
}

bool AppController::desktopNotificationsAvailable() const {
  return platform_services_->DesktopNotificationsAvailable();
}

QString AppController::notificationTestStatus() const {
  return notification_test_status_;
}

void AppController::setScenarioName(const QString &scenarioName) {
  if (snapshot_.scenarioName == scenarioName ||
      !repository_->ScenarioNames().contains(scenarioName)) {
    return;
  }
  LoadScenario(scenarioName);
}

void AppController::setDraftText(const QString &draftText) {
  const QString key = CurrentStateKey();
  if (key.isEmpty() || drafts_.value(key) == draftText) {
    return;
  }
  drafts_[key] = draftText;
  repository_->StoreDraft(CurrentConversationId(), draftText);
  emit draftTextChanged();
}

void AppController::setCurrentSection(const QString &currentSection) {
  static const QStringList sections = {
      QStringLiteral("chats"),
      QStringLiteral("contacts"),
      QStringLiteral("settings"),
  };
  if (current_section_ == currentSection ||
      !sections.contains(currentSection)) {
    return;
  }
  current_section_ = currentSection;
  emit currentSectionChanged();
}

void AppController::selectConversation(int index) {
  const auto *record = conversations_.RecordAt(index);
  if (record == nullptr) {
    return;
  }

  selected_conversation_index_ = index;
  messages_.SetRecords(snapshot_.messagesByConversation.value(
      record->conversationId, QVector<MessageRecord>{}));
  emit selectedConversationChanged();
  emit draftTextChanged();
}

void AppController::sendMessage(const QString &text) {
  const QString trimmed = text.trimmed();
  const auto *conversation =
      conversations_.RecordAt(selected_conversation_index_);
  if (trimmed.isEmpty() || conversation == nullptr) {
    return;
  }

  const auto clientMessageId = next_client_message_id_--;
  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
  MessageRecord message{
      .clientMessageId = clientMessageId,
      .senderId = QStringLiteral("me"),
      .body = trimmed,
      .timestamp = timestamp,
      .outgoing = true,
      .deliveryState = MessageDeliveryState::PendingLocal,
  };

  if (!repository_->EnqueueOutgoing(conversation->conversationId, message)) {
    message.deliveryState = MessageDeliveryState::PermanentFailed;
  }
  const bool shouldSchedule =
      message.deliveryState == MessageDeliveryState::PendingLocal;

  snapshot_.messagesByConversation[conversation->conversationId].push_back(
      message);
  messages_.Append(std::move(message));
  conversations_.UpdatePreview(conversation->conversationId, trimmed,
                               timestamp);
  drafts_.remove(CurrentStateKey());
  emit draftTextChanged();
  if (shouldSchedule && network_enabled_) {
    const auto peerUid = DirectPeerUid(conversation->conversationId);
    if (!peerUid.has_value()) {
      UpdateMessageState(clientMessageId,
                         MessageDeliveryState::PermanentFailed);
      SetServiceActionStatus(tr("当前会话没有可用的服务端接收者"));
      return;
    }
    if (!gateway_client_.IsReady()) {
      UpdateMessageState(clientMessageId, MessageDeliveryState::Unknown);
      SetServiceActionStatus(tr("连接尚未就绪，消息保留为待确认"));
      return;
    }
    UpdateMessageState(clientMessageId, MessageDeliveryState::WaitingAccept);
    const QString wireClientMessageId =
        QStringLiteral("%1:%2").arg(authenticated_uid_).arg(-clientMessageId);
    const std::int64_t remoteConversationId =
        conversation->remoteConversationId.value_or(0);
    const QString requestId = gateway_client_.SendText(
        *peerUid, trimmed.toUtf8(), wireClientMessageId, remoteConversationId);
    outgoing_request_messages_.insert(requestId, clientMessageId);
  } else if (shouldSchedule) {
    ScheduleDeliveryLifecycle(clientMessageId);
  }
}

void AppController::togglePinned(int index) {
  conversations_.TogglePinned(index);
}

void AppController::toggleMuted(int index) {
  conversations_.ToggleMuted(index);
}

void AppController::markRead(int index) {
  if (conversations_.MarkRead(index) && index == selected_conversation_index_) {
    emit selectedConversationChanged();
  }
}

qreal AppController::conversationListPosition() const {
  return conversation_list_position_;
}

void AppController::saveConversationListPosition(qreal position) {
  if (position >= 0) {
    conversation_list_position_ = position;
  }
}

qreal AppController::timelinePosition() const {
  return timeline_positions_.value(CurrentStateKey());
}

void AppController::saveTimelinePosition(qreal position) {
  const QString key = CurrentStateKey();
  if (!key.isEmpty() && position >= 0) {
    timeline_positions_[key] = position;
  }
}

void AppController::selectContact(int index) {
  if (contacts_.RecordAt(index) == nullptr) {
    return;
  }
  selected_contact_index_ = index;
  emit selectedContactChanged();
}

void AppController::toggleContactFavorite(int index) {
  if (contacts_.ToggleFavorite(index) && index == selected_contact_index_) {
    emit selectedContactChanged();
  }
}

void AppController::resolveRequest(int index, bool accepted) {
  const auto *request = requests_.RecordAt(index);
  if (request == nullptr || request->status != QStringLiteral("pending")) {
    return;
  }
  if (!network_enabled_) {
    requests_.SetStatus(index, accepted ? QStringLiteral("accepted")
                                        : QStringLiteral("declined"));
    return;
  }
  if (!gateway_client_.IsReady()) {
    SetServiceActionStatus(tr("连接尚未就绪，暂时不能处理申请"));
    return;
  }

  QString requestId;
  if (request->kind == QStringLiteral("group")) {
    const QStringList fields = request->requestId.split(QLatin1Char(':'));
    if (fields.size() != 3) {
      SetServiceActionStatus(tr("群申请缺少必要标识"));
      return;
    }
    bool groupValid = false;
    bool userValid = false;
    const auto groupId = fields[1].toLongLong(&groupValid);
    const auto userId = fields[2].toLongLong(&userValid);
    if (!groupValid || !userValid) {
      SetServiceActionStatus(tr("群申请标识无效"));
      return;
    }
    requestId = gateway_client_.ReplyJoinGroup(groupId, userId, accepted);
  } else {
    bool valid = false;
    const auto userId = request->requestId.toLongLong(&valid);
    if (!valid || userId <= 0) {
      SetServiceActionStatus(tr("好友申请缺少用户 ID"));
      return;
    }
    requestId = gateway_client_.ReplyFriendRequest(userId, accepted, {});
  }
  friend_reply_requests_.insert(requestId, accepted ? index + 1 : -(index + 1));
}

void AppController::authenticate(const QString &username,
                                 const QString &password) {
  if (!network_enabled_) {
    completeAuthentication();
    return;
  }
  if (authentication_busy_) {
    return;
  }
  if (username.trimmed().isEmpty() || password.isEmpty()) {
    authentication_error_ = tr("请输入账号和密码");
    emit authenticationStateChanged();
    return;
  }
  authentication_busy_ = true;
  authentication_error_.clear();
  emit authenticationStateChanged();
  SetConnectionStatus(QStringLiteral("connecting"));
  gate_client_.SignIn(username.trimmed(), password);
}

void AppController::startConversationWithSelectedContact() {
  const auto *contact = contacts_.RecordAt(selected_contact_index_);
  if (contact == nullptr) {
    return;
  }
  bool valid = false;
  const auto peerUid = contact->userId.toLongLong(&valid);
  if (!valid || peerUid <= 0) {
    if (network_enabled_) {
      SetServiceActionStatus(tr("联系人缺少服务端用户 ID"));
      return;
    }
    setCurrentSection(QStringLiteral("chats"));
    return;
  }
  const QString conversationId = EnsureDirectConversation(
      peerUid, contact->displayName, contact->avatarColor);
  setCurrentSection(QStringLiteral("chats"));
  selectConversation(conversations_.IndexOf(conversationId));
}

void AppController::sendFriendRequest(const QString &uid,
                                      const QString &message) {
  bool valid = false;
  const auto recipientUid = uid.toLongLong(&valid);
  if (!network_enabled_ || !gateway_client_.IsReady() || !valid ||
      recipientUid <= 0) {
    SetServiceActionStatus(tr("请输入有效用户 ID，并确认连接已就绪"));
    return;
  }
  gateway_client_.SendFriendRequest(recipientUid, message.trimmed());
  SetServiceActionStatus(tr("好友申请已提交"));
}

void AppController::createGroup(const QString &name) {
  if (!network_enabled_ || !gateway_client_.IsReady() ||
      name.trimmed().isEmpty()) {
    SetServiceActionStatus(tr("群名不能为空，且连接必须已就绪"));
    return;
  }
  gateway_client_.CreateGroup(name.trimmed());
  SetServiceActionStatus(tr("创建群请求已提交"));
}

void AppController::joinGroup(const QString &groupId, const QString &message) {
  bool valid = false;
  const auto numericGroupId = groupId.toLongLong(&valid);
  if (!network_enabled_ || !gateway_client_.IsReady() || !valid ||
      numericGroupId <= 0) {
    SetServiceActionStatus(tr("请输入有效群 ID，并确认连接已就绪"));
    return;
  }
  gateway_client_.RequestJoinGroup(numericGroupId, message.trimmed());
  SetServiceActionStatus(tr("入群申请已提交"));
}

void AppController::completeAuthentication() {
  if (!authRequired()) {
    return;
  }
  authentication_completed_ = true;
  SetConnectionStatus(QStringLiteral("online"));
  emit authRequiredChanged();
}

void AppController::sendTestDesktopNotification() {
  const QString nextStatus =
      platform_services_->ShowDesktopNotification(
          tr("WIM 桌面通知"), tr("Linux 平台通知端口工作正常。"))
          ? QStringLiteral("sent")
          : QStringLiteral("unavailable");
  if (notification_test_status_ == nextStatus) {
    return;
  }
  notification_test_status_ = nextStatus;
  emit notificationTestStatusChanged();
}

void AppController::ConfigureNetwork(const QString &gateUrl) {
  gate_client_.SetBaseUrl(QUrl(gateUrl));

  connect(&gate_client_, &GateHttpClient::SignInSucceeded, this,
          [this](const GateSession &session) {
            authenticated_uid_ = session.uid;
            SetConnectionStatus(QStringLiteral("connecting"));
            gateway_client_.Open(session);
          });
  connect(&gate_client_, &GateHttpClient::OperationFailed, this,
          [this](const QString &operation, int, const QString &message) {
            if (operation != QStringLiteral("sign-in")) {
              SetServiceActionStatus(message);
              return;
            }
            authentication_busy_ = false;
            authentication_error_ = message;
            SetConnectionStatus(QStringLiteral("auth-expired"));
            emit authenticationStateChanged();
          });

  connect(&gateway_client_, &ConnectionGatewayClient::Authenticated, this,
          [this] {
            authentication_busy_ = false;
            authentication_error_.clear();
            authentication_completed_ = true;
            SetConnectionStatus(QStringLiteral("online"));
            emit authenticationStateChanged();
            emit authRequiredChanged();
            gateway_client_.PullFriendList();
            gateway_client_.PullFriendApplications();
            SyncKnownConversations();
            ResumePendingOutgoing();
          });
  connect(&gateway_client_, &ConnectionGatewayClient::CredentialsExpired, this,
          [this] {
            authentication_completed_ = false;
            authentication_busy_ = false;
            authentication_error_ = tr("登录凭证已过期，请重新登录");
            SetConnectionStatus(QStringLiteral("auth-expired"));
            emit authenticationStateChanged();
            emit authRequiredChanged();
          });
  connect(&gateway_client_, &ConnectionGatewayClient::StateChanged, this,
          [this](ConnectionGatewayClient::State state) {
            if (state == ConnectionGatewayClient::State::Connecting ||
                state == ConnectionGatewayClient::State::Authenticating) {
              SetConnectionStatus(QStringLiteral("connecting"));
            } else if (state == ConnectionGatewayClient::State::Reconnecting) {
              SetConnectionStatus(QStringLiteral("reconnecting"));
            } else if (state == ConnectionGatewayClient::State::Disconnected &&
                       authentication_completed_) {
              SetConnectionStatus(QStringLiteral("offline-cached"));
            }
          });
  connect(&gateway_client_, &ConnectionGatewayClient::ResponseReceived, this,
          &AppController::HandleGatewayResponse);
  connect(&gateway_client_, &ConnectionGatewayClient::PushReceived, this,
          &AppController::HandleGatewayPush);
  connect(&gateway_client_, &ConnectionGatewayClient::RequestFailed, this,
          &AppController::HandleGatewayFailure);
  connect(&gateway_client_, &ConnectionGatewayClient::ProtocolError, this,
          [this](const QString &message) { SetServiceActionStatus(message); });

  authentication_completed_ = false;
  SetConnectionStatus(QStringLiteral("auth-required"));
  emit authRequiredChanged();
}

void AppController::HandleGatewayResponse(const QString &requestId,
                                          quint32 serviceId,
                                          const QByteArray &payload) {
  wim::protocol::Packet response;
  if (!ParsePacket(payload, &response)) {
    SetServiceActionStatus(tr("服务端响应无法解析"));
    return;
  }
  const int errorCode = response.has_error() ? response.error() : 0;

  if (serviceId == protocol::SendTextResponse ||
      serviceId == protocol::SendGroupTextResponse) {
    const auto found = outgoing_request_messages_.find(requestId);
    if (found == outgoing_request_messages_.end()) {
      return;
    }
    const std::int64_t clientMessageId = found.value();
    outgoing_request_messages_.erase(found);
    if (errorCode != protocol::Success || !response.has_message_id() ||
        !response.has_conversation_id() || !response.has_conversation_seq()) {
      UpdateMessageState(
          clientMessageId,
          response.retryable() || protocol::IsRetryableError(errorCode)
              ? MessageDeliveryState::RetryableFailed
              : MessageDeliveryState::PermanentFailed);
      SetServiceActionStatus(
          response.has_message()
              ? QString::fromStdString(response.message())
              : tr("消息发送被服务端拒绝（%1）").arg(errorCode));
      return;
    }

    QString localConversationId;
    for (auto iterator = snapshot_.messagesByConversation.begin();
         iterator != snapshot_.messagesByConversation.end(); ++iterator) {
      for (auto &message : iterator.value()) {
        if (message.clientMessageId != clientMessageId) {
          continue;
        }
        message.messageId = response.message_id();
        message.conversationSeq = response.conversation_seq();
        message.deliveryState = MessageDeliveryState::Accepted;
        localConversationId = iterator.key();
        break;
      }
      if (!localConversationId.isEmpty()) {
        break;
      }
    }
    if (localConversationId.isEmpty()) {
      return;
    }
    repository_->AcceptOutgoing(clientMessageId, response.message_id(),
                                localConversationId,
                                response.conversation_seq());
    messages_.UpdateDeliveryState(clientMessageId,
                                  MessageDeliveryState::Accepted);
    AttachRemoteConversation(localConversationId, response.conversation_id());
    return;
  }

  if (serviceId == protocol::PullFriendListResponse) {
    if (errorCode != protocol::Success) {
      SetServiceActionStatus(tr("拉取好友列表失败（%1）").arg(errorCode));
      return;
    }
    QVector<ContactRecord> contacts;
    contacts.reserve(response.friend_list_size());
    static const QStringList colors = {
        QStringLiteral("#315FD6"), QStringLiteral("#7656A8"),
        QStringLiteral("#247B6B"), QStringLiteral("#A05A4E")};
    for (const auto &friendInfo : response.friend_list()) {
      const QString uid = QString::number(friendInfo.uid());
      bool favorite = false;
      for (const auto &existing : snapshot_.contacts) {
        if (existing.userId == uid) {
          favorite = existing.favorite;
          break;
        }
      }
      contacts.push_back(ContactRecord{
          .userId = uid,
          .displayName = friendInfo.name().empty()
                             ? tr("用户 %1").arg(uid)
                             : QString::fromStdString(friendInfo.name()),
          .statusText = tr("WIM 用户 · %1").arg(uid),
          .avatarColor = colors[friendInfo.uid() % colors.size()],
          .online = false,
          .favorite = favorite,
      });
    }
    snapshot_.contacts = contacts;
    contacts_.SetRecords(contacts);
    selected_contact_index_ = contacts.isEmpty() ? -1 : 0;
    repository_->ReplaceContacts(contacts);
    emit selectedContactChanged();
    return;
  }

  if (serviceId == protocol::PullFriendApplyListResponse) {
    if (errorCode != protocol::Success) {
      SetServiceActionStatus(tr("拉取好友申请失败（%1）").arg(errorCode));
      return;
    }
    QVector<RequestRecord> requests;
    requests.reserve(response.apply_list_size());
    for (const auto &apply : response.apply_list()) {
      const QString from = QString::number(apply.from());
      const QString status = apply.status() == 0   ? QStringLiteral("pending")
                             : apply.status() == 1 ? QStringLiteral("accepted")
                                                   : QStringLiteral("declined");
      requests.push_back(RequestRecord{
          .requestId = from,
          .displayName = tr("用户 %1").arg(from),
          .message = QString::fromStdString(apply.content()),
          .avatarColor = QStringLiteral("#367A91"),
          .kind = QStringLiteral("friend"),
          .status = status,
      });
    }
    snapshot_.requests = requests;
    requests_.SetRecords(requests);
    repository_->ReplaceRequests(requests);
    return;
  }

  if (serviceId == protocol::PullSessionMessagesResponse) {
    const QString localConversationId =
        sync_request_conversations_.take(requestId);
    if (localConversationId.isEmpty() || errorCode != protocol::Success) {
      if (errorCode != protocol::Success) {
        SetServiceActionStatus(tr("同步会话失败（%1）").arg(errorCode));
      }
      return;
    }
    AttachRemoteConversation(localConversationId, response.conversation_id());
    auto &stored = snapshot_.messagesByConversation[localConversationId];
    QVector<MessageRecord> additions;
    for (const auto &item : response.message_list()) {
      const auto duplicate =
          std::find_if(stored.cbegin(), stored.cend(),
                       [&item](const MessageRecord &message) {
                         return message.messageId == item.message_id();
                       });
      if (duplicate != stored.cend()) {
        continue;
      }
      additions.push_back(MessageRecord{
          .clientMessageId = item.message_id(),
          .messageId = item.message_id(),
          .conversationSeq = item.conversation_seq(),
          .senderId = QString::number(item.from()),
          .body = QString::fromStdString(item.content()),
          .timestamp = DisplayTimestamp(item.send_date_time()),
          .outgoing = item.from() == authenticated_uid_,
          .deliveryState = item.status() >= 3 ? MessageDeliveryState::Read
                           : item.status() >= 2
                               ? MessageDeliveryState::Delivered
                               : MessageDeliveryState::Accepted,
      });
    }
    const std::int64_t nextCursor =
        response.has_next_seq() ? response.next_seq()
                                : repository_->SyncCursor(localConversationId);
    if (!repository_->ApplyIncomingBatch(localConversationId, additions,
                                         nextCursor)) {
      SetServiceActionStatus(tr("同步消息写入本地数据库失败"));
      return;
    }
    for (const auto &message : additions) {
      stored.push_back(message);
      if (!message.outgoing && message.messageId.has_value() &&
          message.conversationSeq.has_value()) {
        gateway_client_.AcknowledgeDelivered(*message.messageId,
                                             response.conversation_id(),
                                             *message.conversationSeq);
      }
    }
    if (CurrentConversationId() == localConversationId) {
      messages_.SetRecords(stored);
    }
    if (response.has_more() && response.has_next_seq()) {
      const QString nextRequest = gateway_client_.PullConversationMessages(
          response.conversation_id(), response.next_seq());
      sync_request_conversations_.insert(nextRequest, localConversationId);
    }
    return;
  }

  const auto reply = friend_reply_requests_.find(requestId);
  if (reply != friend_reply_requests_.end()) {
    const int encodedIndex = reply.value();
    friend_reply_requests_.erase(reply);
    if (errorCode == protocol::Success) {
      const int index = std::abs(encodedIndex) - 1;
      requests_.SetStatus(index, encodedIndex > 0 ? QStringLiteral("accepted")
                                                  : QStringLiteral("declined"));
      gateway_client_.PullFriendList();
      SetServiceActionStatus(tr("申请已处理"));
    } else {
      SetServiceActionStatus(tr("处理申请失败（%1）").arg(errorCode));
    }
    return;
  }

  SetServiceActionStatus(errorCode == protocol::Success
                             ? tr("服务请求已完成")
                             : tr("服务请求失败（%1）").arg(errorCode));
}

void AppController::HandleGatewayPush(quint32 serviceId,
                                      const QByteArray &payload) {
  wim::protocol::Packet packet;
  if (!ParsePacket(payload, &packet)) {
    SetServiceActionStatus(tr("收到无法解析的推送"));
    return;
  }

  if (serviceId == protocol::SendTextRequest ||
      serviceId == protocol::SendGroupTextRequest) {
    const bool group = serviceId == protocol::SendGroupTextRequest;
    const QString localConversationId =
        group ? QStringLiteral("group:%1").arg(packet.gid())
              : EnsureDirectConversation(packet.from(),
                                         tr("用户 %1").arg(packet.from()),
                                         QStringLiteral("#315FD6"));
    if (group && conversations_.IndexOf(localConversationId) < 0) {
      ConversationRecord conversation{
          .conversationId = localConversationId,
          .title = tr("群聊 %1").arg(packet.gid()),
          .preview = QString{},
          .timestamp = QString{},
          .avatarColor = QStringLiteral("#7656A8"),
          .remoteConversationId =
              packet.has_conversation_id()
                  ? std::optional<std::int64_t>(packet.conversation_id())
                  : std::nullopt,
      };
      repository_->SaveConversation(conversation);
      snapshot_.conversations.prepend(conversation);
      snapshot_.messagesByConversation.insert(localConversationId, {});
      if (selected_conversation_index_ >= 0) {
        ++selected_conversation_index_;
      }
      conversations_.Prepend(std::move(conversation));
    }
    if (packet.has_conversation_id()) {
      AttachRemoteConversation(localConversationId, packet.conversation_id());
    }
    const std::int64_t messageId =
        packet.has_message_id() ? packet.message_id() : packet.seq();
    auto &stored = snapshot_.messagesByConversation[localConversationId];
    const bool duplicate =
        std::any_of(stored.cbegin(), stored.cend(),
                    [messageId](const MessageRecord &message) {
                      return message.messageId == messageId;
                    });
    if (duplicate) {
      gateway_client_.AcknowledgeTransport(messageId);
      return;
    }
    MessageRecord incoming{
        .clientMessageId = messageId,
        .messageId = messageId,
        .conversationSeq =
            packet.has_conversation_seq()
                ? std::optional<std::int64_t>(packet.conversation_seq())
                : std::nullopt,
        .senderId = QString::number(packet.from()),
        .body = QString::fromUtf8(packet.data().data(), packet.data().size()),
        .timestamp = DisplayTimestamp(packet.send_date_time()),
        .outgoing = false,
        .deliveryState = MessageDeliveryState::Delivered,
    };
    const std::int64_t cursor = incoming.conversationSeq.value_or(
        repository_->SyncCursor(localConversationId));
    if (!repository_->ApplyIncomingBatch(localConversationId, {incoming},
                                         cursor)) {
      SetServiceActionStatus(tr("推送消息写入本地数据库失败"));
      return;
    }
    stored.push_back(incoming);
    if (CurrentConversationId() == localConversationId) {
      messages_.Append(incoming);
    }
    conversations_.UpdatePreview(localConversationId, incoming.body,
                                 incoming.timestamp);
    gateway_client_.AcknowledgeTransport(messageId);
    if (packet.has_conversation_id() && packet.has_conversation_seq()) {
      gateway_client_.AcknowledgeDelivered(messageId, packet.conversation_id(),
                                           packet.conversation_seq());
    }
    return;
  }

  if (serviceId == protocol::AddFriendRequest ||
      serviceId == protocol::JoinGroupRequest) {
    RequestRecord request{
        .requestId = serviceId == protocol::JoinGroupRequest
                         ? QStringLiteral("group:%1:%2")
                               .arg(packet.gid())
                               .arg(packet.uid())
                         : QString::number(packet.from()),
        .displayName = tr("用户 %1").arg(serviceId == protocol::JoinGroupRequest
                                             ? packet.uid()
                                             : packet.from()),
        .message = serviceId == protocol::JoinGroupRequest
                       ? QString::fromStdString(packet.content())
                       : QString::fromStdString(packet.request_message()),
        .avatarColor = QStringLiteral("#367A91"),
        .kind = serviceId == protocol::JoinGroupRequest
                    ? QStringLiteral("group")
                    : QStringLiteral("friend"),
        .status = QStringLiteral("pending"),
    };
    snapshot_.requests.push_back(std::move(request));
    requests_.SetRecords(snapshot_.requests);
    repository_->ReplaceRequests(snapshot_.requests);
    if (packet.has_seq()) {
      gateway_client_.AcknowledgeTransport(packet.seq());
    }
    return;
  }

  if (serviceId == protocol::ReplyFriendRequest ||
      serviceId == protocol::ReplyJoinGroupRequest) {
    if (packet.has_seq()) {
      gateway_client_.AcknowledgeTransport(packet.seq());
    }
    gateway_client_.PullFriendList();
    gateway_client_.PullFriendApplications();
  }
}

void AppController::HandleGatewayFailure(const QString &requestId,
                                         quint32 serviceId, int errorCode,
                                         const QString &message,
                                         bool outcomeUnknown) {
  const auto outgoing = outgoing_request_messages_.find(requestId);
  if (outgoing != outgoing_request_messages_.end()) {
    const auto clientMessageId = outgoing.value();
    outgoing_request_messages_.erase(outgoing);
    UpdateMessageState(clientMessageId,
                       outcomeUnknown ? MessageDeliveryState::Unknown
                       : protocol::IsRetryableError(errorCode)
                           ? MessageDeliveryState::RetryableFailed
                           : MessageDeliveryState::PermanentFailed);
  }
  sync_request_conversations_.remove(requestId);
  friend_reply_requests_.remove(requestId);
  SetServiceActionStatus(tr("请求 %1 失败：%2").arg(serviceId).arg(message));
}

void AppController::SyncKnownConversations() {
  for (const auto &conversation : snapshot_.conversations) {
    if (!conversation.remoteConversationId.has_value()) {
      continue;
    }
    const QString requestId = gateway_client_.PullConversationMessages(
        *conversation.remoteConversationId,
        repository_->SyncCursor(conversation.conversationId));
    sync_request_conversations_.insert(requestId, conversation.conversationId);
  }
}

void AppController::ResumePendingOutgoing() {
  for (auto iterator = snapshot_.messagesByConversation.begin();
       iterator != snapshot_.messagesByConversation.end(); ++iterator) {
    const auto peerUid = DirectPeerUid(iterator.key());
    if (!peerUid.has_value()) {
      continue;
    }
    const int conversationIndex = conversations_.IndexOf(iterator.key());
    const auto *conversation = conversations_.RecordAt(conversationIndex);
    const std::int64_t remoteConversationId =
        conversation == nullptr
            ? 0
            : conversation->remoteConversationId.value_or(0);
    for (auto &message : iterator.value()) {
      if (!message.outgoing ||
          (message.deliveryState != MessageDeliveryState::PendingLocal &&
           message.deliveryState != MessageDeliveryState::WaitingAccept &&
           message.deliveryState != MessageDeliveryState::Unknown &&
           message.deliveryState != MessageDeliveryState::RetryableFailed)) {
        continue;
      }
      if (message.deliveryState != MessageDeliveryState::WaitingAccept) {
        UpdateMessageState(message.clientMessageId,
                           MessageDeliveryState::WaitingAccept);
      }
      const QString wireClientMessageId = QStringLiteral("%1:%2")
                                              .arg(authenticated_uid_)
                                              .arg(-message.clientMessageId);
      const QString requestId =
          gateway_client_.SendText(*peerUid, message.body.toUtf8(),
                                   wireClientMessageId, remoteConversationId);
      outgoing_request_messages_.insert(requestId, message.clientMessageId);
    }
  }
}

QString AppController::EnsureDirectConversation(std::int64_t peerUid,
                                                const QString &title,
                                                const QString &avatarColor) {
  const QString conversationId = DirectConversationId(peerUid);
  if (conversations_.IndexOf(conversationId) >= 0) {
    return conversationId;
  }
  ConversationRecord conversation{
      .conversationId = conversationId,
      .title = title,
      .preview = QString{},
      .timestamp = QString{},
      .avatarColor = avatarColor,
  };
  if (!repository_->SaveConversation(conversation)) {
    SetServiceActionStatus(tr("无法创建本地会话"));
    return {};
  }
  snapshot_.conversations.prepend(conversation);
  snapshot_.messagesByConversation.insert(conversationId, {});
  if (selected_conversation_index_ >= 0) {
    ++selected_conversation_index_;
  }
  conversations_.Prepend(std::move(conversation));
  return conversationId;
}

void AppController::AttachRemoteConversation(
    const QString &localConversationId, std::int64_t remoteConversationId) {
  if (remoteConversationId <= 0) {
    return;
  }
  for (auto &conversation : snapshot_.conversations) {
    if (conversation.conversationId != localConversationId) {
      continue;
    }
    if (conversation.remoteConversationId == remoteConversationId) {
      return;
    }
    conversation.remoteConversationId = remoteConversationId;
    conversations_.SetRemoteConversationId(localConversationId,
                                           remoteConversationId);
    repository_->SetRemoteConversationId(localConversationId,
                                         remoteConversationId);
    return;
  }
}

void AppController::SetConnectionStatus(const QString &status) {
  if (snapshot_.connectionStatus == status) {
    return;
  }
  snapshot_.connectionStatus = status;
  emit connectionStatusChanged();
}

void AppController::SetServiceActionStatus(const QString &status) {
  if (service_action_status_ == status) {
    return;
  }
  service_action_status_ = status;
  emit serviceActionStatusChanged();
}

void AppController::LoadScenario(const QString &scenarioName) {
  snapshot_ = repository_->LoadScenario(scenarioName);
  next_client_message_id_ = -1000;
  for (auto iterator = snapshot_.messagesByConversation.cbegin();
       iterator != snapshot_.messagesByConversation.cend(); ++iterator) {
    for (const auto &message : iterator.value()) {
      next_client_message_id_ =
          std::min(next_client_message_id_, message.clientMessageId - 1);
    }
  }
  conversations_.SetRecords(snapshot_.conversations);
  contacts_.SetRecords(snapshot_.contacts);
  requests_.SetRecords(snapshot_.requests);
  for (auto iterator = snapshot_.draftsByConversation.cbegin();
       iterator != snapshot_.draftsByConversation.cend(); ++iterator) {
    drafts_[snapshot_.scenarioName + QStringLiteral(":") + iterator.key()] =
        iterator.value();
  }
  selected_conversation_index_ = snapshot_.conversations.isEmpty() ? -1 : 0;
  selected_contact_index_ = snapshot_.contacts.isEmpty() ? -1 : 0;
  authentication_completed_ = false;

  if (selected_conversation_index_ >= 0) {
    const auto &conversation = snapshot_.conversations.front();
    messages_.SetRecords(snapshot_.messagesByConversation.value(
        conversation.conversationId, QVector<MessageRecord>{}));
  } else {
    messages_.SetRecords({});
  }

  emit scenarioNameChanged();
  emit connectionStatusChanged();
  emit selectedConversationChanged();
  emit selectedContactChanged();
  emit draftTextChanged();
  emit authRequiredChanged();
}

void AppController::ScheduleDeliveryLifecycle(std::int64_t clientMessageId) {
  struct Transition {
    int delay;
    MessageDeliveryState state;
  };
  const Transition transitions[] = {
      {350, MessageDeliveryState::WaitingAccept},
      {900, MessageDeliveryState::Accepted},
      {1500, MessageDeliveryState::Delivered},
      {2300, MessageDeliveryState::Read},
  };

  for (const auto &transition : transitions) {
    QTimer::singleShot(transition.delay, this,
                       [this, clientMessageId, state = transition.state] {
                         UpdateMessageState(clientMessageId, state);
                       });
  }
}

void AppController::UpdateMessageState(std::int64_t clientMessageId,
                                       MessageDeliveryState state) {
  bool changed = false;
  for (auto iterator = snapshot_.messagesByConversation.begin();
       iterator != snapshot_.messagesByConversation.end(); ++iterator) {
    for (auto &message : iterator.value()) {
      if (message.clientMessageId != clientMessageId ||
          !CanTransition(message.deliveryState, state)) {
        continue;
      }
      message.deliveryState = state;
      changed = true;
      break;
    }
    if (changed) {
      break;
    }
  }

  if (changed) {
    repository_->UpdateDeliveryState(clientMessageId, state);
    messages_.UpdateDeliveryState(clientMessageId, state);
  }
}

QString AppController::CurrentConversationId() const {
  const auto *record = conversations_.RecordAt(selected_conversation_index_);
  return record == nullptr ? QString{} : record->conversationId;
}

QString AppController::CurrentStateKey() const {
  const QString conversationId = CurrentConversationId();
  if (conversationId.isEmpty()) {
    return {};
  }
  return snapshot_.scenarioName + QStringLiteral(":") + conversationId;
}

QString AppController::ArgumentValue(const QString &name,
                                     const QString &fallback) {
  const QString prefix = QStringLiteral("--") + name + QStringLiteral("=");
  const auto arguments = QCoreApplication::arguments();
  for (const auto &argument : arguments) {
    if (argument.startsWith(prefix)) {
      return argument.mid(prefix.size());
    }
  }
  return fallback;
}

}  // namespace wim::client
