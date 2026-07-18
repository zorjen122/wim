#pragma once

#include "tcp_message.qpb.h"

#include <QByteArray>
#include <QString>

namespace wim::client {

bool SerializeProtobufPacket(const wim::protocol::Packet &packet,
                             QByteArray *payload);
bool ParseProtobufPacket(const QByteArray &payload,
                         wim::protocol::Packet *packet);
QString PacketSendDateTimeOrEmpty(const wim::protocol::Packet &packet);

}  // namespace wim::client
