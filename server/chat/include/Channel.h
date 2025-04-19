#pragma once

#include <memory>
#include <queue>
#include <unordered_map>

#define PREFIX_FRIEND_CHANNEL_ID 100000

namespace wim {
class Channel : public std::enable_shared_from_this<Channel> {
public:
  using Ptr = std::shared_ptr<Channel>;
  enum Type { FRIEND, GROUP };

  Channel() = delete;

  Channel(int from, int to, Type type);
  ~Channel();

  int getId() const { return id; }
  Type getType() const { return type; }
  int getFrom() const { return from; }
  int getTo() const { return to; }

private:
  int id;
  Type type;
  std::queue<size_t> messageSequeueId;
  // 会话的映射，或是uid、或是gid
  int from;
  int to;
};

static std::unordered_map<size_t, std::unordered_map<size_t, Channel::Ptr>>
    channelManager;
}; // namespace wim