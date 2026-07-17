#include "models/MessageListModel.h"

#include <utility>

namespace wim::client {

QString DeliveryStateName(MessageDeliveryState state) {
  switch (state) {
    case MessageDeliveryState::PendingLocal:
      return QStringLiteral("pending");
    case MessageDeliveryState::WaitingAccept:
      return QStringLiteral("waiting-accept");
    case MessageDeliveryState::Accepted:
      return QStringLiteral("accepted");
    case MessageDeliveryState::Delivered:
      return QStringLiteral("delivered");
    case MessageDeliveryState::Read:
      return QStringLiteral("read");
    case MessageDeliveryState::Unknown:
      return QStringLiteral("unknown");
    case MessageDeliveryState::RetryableFailed:
      return QStringLiteral("retryable-failed");
    case MessageDeliveryState::PermanentFailed:
      return QStringLiteral("permanent-failed");
  }
  return QStringLiteral("unknown");
}

bool CanTransition(MessageDeliveryState from, MessageDeliveryState to) {
  if (from == to) {
    return false;
  }

  switch (from) {
    case MessageDeliveryState::PendingLocal:
      return to == MessageDeliveryState::WaitingAccept ||
             to == MessageDeliveryState::Unknown ||
             to == MessageDeliveryState::RetryableFailed ||
             to == MessageDeliveryState::PermanentFailed;
    case MessageDeliveryState::WaitingAccept:
      return to == MessageDeliveryState::Accepted ||
             to == MessageDeliveryState::Unknown ||
             to == MessageDeliveryState::RetryableFailed ||
             to == MessageDeliveryState::PermanentFailed;
    case MessageDeliveryState::Accepted:
      return to == MessageDeliveryState::Delivered ||
             to == MessageDeliveryState::Read;
    case MessageDeliveryState::Delivered:
      return to == MessageDeliveryState::Read;
    case MessageDeliveryState::Unknown:
      return to == MessageDeliveryState::WaitingAccept ||
             to == MessageDeliveryState::Accepted ||
             to == MessageDeliveryState::Delivered ||
             to == MessageDeliveryState::Read ||
             to == MessageDeliveryState::RetryableFailed ||
             to == MessageDeliveryState::PermanentFailed;
    case MessageDeliveryState::RetryableFailed:
      return to == MessageDeliveryState::PendingLocal ||
             to == MessageDeliveryState::WaitingAccept ||
             to == MessageDeliveryState::Accepted ||
             to == MessageDeliveryState::PermanentFailed;
    case MessageDeliveryState::Read:
    case MessageDeliveryState::PermanentFailed:
      return false;
  }
  return false;
}

MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int MessageListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : records_.size();
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= records_.size()) {
    return {};
  }

  const auto &record = records_.at(index.row());
  switch (role) {
    case ClientMessageIdRole:
      return QVariant::fromValue<qlonglong>(record.clientMessageId);
    case MessageIdRole:
      return record.messageId.has_value()
                 ? QVariant::fromValue<qlonglong>(*record.messageId)
                 : QVariant{};
    case ConversationSeqRole:
      return record.conversationSeq.has_value()
                 ? QVariant::fromValue<qlonglong>(*record.conversationSeq)
                 : QVariant{};
    case SenderIdRole:
      return record.senderId;
    case BodyRole:
      return record.body;
    case TimestampRole:
      return record.timestamp;
    case OutgoingRole:
      return record.outgoing;
    case DeliveryStateRole:
      return DeliveryStateName(record.deliveryState);
    default:
      return {};
  }
}

QHash<int, QByteArray> MessageListModel::roleNames() const {
  return {
      {ClientMessageIdRole, "clientMessageId"},
      {MessageIdRole, "messageId"},
      {ConversationSeqRole, "conversationSeq"},
      {SenderIdRole, "senderId"},
      {BodyRole, "body"},
      {TimestampRole, "timestamp"},
      {OutgoingRole, "outgoing"},
      {DeliveryStateRole, "deliveryState"},
  };
}

void MessageListModel::SetRecords(QVector<MessageRecord> records) {
  beginResetModel();
  records_ = std::move(records);
  endResetModel();
}

void MessageListModel::Append(MessageRecord record) {
  const int row = records_.size();
  beginInsertRows({}, row, row);
  records_.push_back(std::move(record));
  endInsertRows();
}

bool MessageListModel::UpdateDeliveryState(std::int64_t clientMessageId,
                                           MessageDeliveryState state) {
  for (int row = 0; row < records_.size(); ++row) {
    auto &record = records_[row];
    if (record.clientMessageId != clientMessageId) {
      continue;
    }
    if (!CanTransition(record.deliveryState, state)) {
      return false;
    }
    record.deliveryState = state;
    const auto modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {DeliveryStateRole});
    return true;
  }
  return false;
}

}  // namespace wim::client
