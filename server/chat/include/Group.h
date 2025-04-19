#pragma once

#include "Channel.h"
#include "ChatSession.h"
#include <cstddef>
#include <json/json.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wim {
class Group {
public:
  Group() { channel = nullptr; }
  Group(size_t id, size_t up, const std::vector<size_t> &numbers) {
    this->channel = nullptr, this->id = id, this->up = up,
    this->numbers = numbers;
  }

  bool setChannel(int uid) {
    this->channel = std::make_shared<Channel>(uid, id, Channel::Type::GROUP);

    if (channelManager[uid][id] != nullptr) {
      return false;
    }

    channelManager[uid][id] = channel;
    return true;
  }

  using Ptr = std::shared_ptr<Group>;
  size_t id;
  size_t up;
  std::vector<size_t> numbers;

  Channel::Ptr channel;
};

namespace dev {
static std::unordered_map<size_t, Group> gg;
};

void GroupCreate(std::shared_ptr<ChatSession> session, unsigned int msgID,
                 const Json::Value &request);
void GroupJoin(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const Json::Value &request);
// TEXT todo...
void GroupQuit(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const Json::Value &request);
void GroupTextSend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                   const Json::Value &request);
}; // namespace wim