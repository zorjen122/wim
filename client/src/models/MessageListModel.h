#pragma once

#include "domain/ClientSnapshot.h"

#include <QAbstractListModel>

namespace wim::client {

class MessageListModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    ClientMessageIdRole = Qt::UserRole + 1,
    MessageIdRole,
    ConversationSeqRole,
    SenderIdRole,
    BodyRole,
    TimestampRole,
    OutgoingRole,
    DeliveryStateRole,
  };
  Q_ENUM(Role)

  explicit MessageListModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void SetRecords(QVector<MessageRecord> records);
  void Append(MessageRecord record);
  bool UpdateDeliveryState(std::int64_t clientMessageId,
                           MessageDeliveryState state);

 private:
  QVector<MessageRecord> records_;
};

}  // namespace wim::client
