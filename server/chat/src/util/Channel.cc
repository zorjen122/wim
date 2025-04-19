#include "Channel.h"
#include <atomic>

namespace wim {
Channel::Channel(int from, int to, Channel::Type type) {

  if (type == FRIEND) {
    id = 0; // todo...
  } else if (type == GROUP) {
    id = to;
  }

  this->type = type;
  this->from = from;
  this->to = to;

  // for friend <a, b>, <b, a> => channel, for group <gid, uid>, <uid, gid> =>
  // channel
  channelManager[from][to] = shared_from_this();
  channelManager[to][from] = shared_from_this();
}

Channel::~Channel() {}
}; // namespace wim