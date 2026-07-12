#pragma once

#include <string>

#include "Const.h"
#include "tcp_message.pb.h"

namespace wim {

using TcpPacket = protocol::Packet;

inline bool ParseTcpPacket(const std::string &data, TcpPacket &packet) {
  return packet.ParseFromString(data);
}

inline std::string SerializeTcpPacket(const TcpPacket &packet) {
  std::string data;
  packet.SerializeToString(&data);
  return data;
}

inline TcpPacket MakeErrorPacket(int error, const std::string &message = {}) {
  TcpPacket packet;
  packet.set_error(error);
  if (!message.empty()) {
    packet.set_message(message);
  }
  return packet;
}

inline int TcpPacketError(const TcpPacket &packet) {
  return packet.has_error() ? packet.error() : ErrorCodes::Success;
}

inline std::string TcpPacketDebugString(const TcpPacket &packet) {
  return packet.ShortDebugString();
}

}  // namespace wim
