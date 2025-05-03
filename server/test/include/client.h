#pragma once
#include <memory>
#include <string>

namespace wim {

class Chat;
class Gate;

struct Client {

  long uid;
  std::string username;
  std::string password;

  std::shared_ptr<Gate> gate;
  std::shared_ptr<Chat> chat;
};
}; // namespace wim
