#include "adapters/connection_gateway/ProtobufPacketCodec.h"

#include <QProtobufSerializer>

namespace wim::client {

bool SerializeProtobufPacket(const wim::protocol::Packet &packet,
                             QByteArray *payload) {
  if (payload == nullptr) {
    return false;
  }
  QProtobufSerializer serializer;
  *payload = packet.serialize(&serializer);
  return serializer.lastError() == QAbstractProtobufSerializer::Error::None;
}

bool ParseProtobufPacket(const QByteArray &payload,
                         wim::protocol::Packet *packet) {
  if (packet == nullptr) {
    return false;
  }
  QProtobufSerializer serializer;
  return packet->deserialize(&serializer, payload);
}

QString PacketSendDateTimeOrEmpty(const wim::protocol::Packet &packet) {
  return packet.hasSendDateTime() ? packet.sendDateTime() : QString{};
}

}  // namespace wim::client
