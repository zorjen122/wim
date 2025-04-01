#pragma once

#include "ChatSession.h"
#include <memory>

class Message : public std::enable_shared_from_this<Message> {
public:
  using Ptr = std::shared_ptr<Message>;
  enum Type {
    Text,
    // todo..
    File,
    Image,
    Video,
    Audio,
  };

private:
  int seq = 0;
  ChatSession::LogicPackage package;
};