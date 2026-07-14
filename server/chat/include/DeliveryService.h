#pragma once

#include <cstdint>
#include <string>

namespace wim {

class DeliveryService {
 public:
  enum class Location { Local, Remote, Offline };

  struct Target {
    Location location{Location::Offline};
    std::string machineId;
  };

  struct RemoteResult {
    bool nodeFound{false};
    bool success{false};
    int error{0};
    int64_t messageId{0};
    std::string status;
  };

  Target Locate(int64_t uid) const;
  bool SendLocal(int64_t uid, const std::string &packet,
                 uint32_t protocolId) const;
  bool SendLocalReliable(int64_t uid, int64_t seq, const std::string &packet,
                         uint32_t protocolId) const;
  bool DeliverFriendApplyLocal(int64_t from, int64_t to,
                               const std::string &message) const;
  bool DeliverFriendReplyLocal(int64_t from, int64_t to, bool accept,
                               const std::string &message) const;
  void Acknowledge(int64_t uid, int64_t seq) const;
  RemoteResult ForwardFriendApply(const std::string &machineId, int64_t from,
                                  int64_t to, const std::string &message) const;
  RemoteResult ForwardFriendReply(const std::string &machineId, int64_t from,
                                  int64_t to, bool accept,
                                  const std::string &message) const;
  RemoteResult ForwardText(const std::string &machineId, int64_t from,
                           int64_t to, const std::string &text, int64_t seq,
                           int64_t sessionKey,
                           const std::string &requestId) const;
};

}  // namespace wim
