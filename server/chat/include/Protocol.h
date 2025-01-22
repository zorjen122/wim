#pragma once
#include "Const.h"
#include <boost/asio.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>

using boost::asio::ip::tcp;
class ServiceSystem;
class ChatSession;

namespace protocol {
class LogicPackage;

class Package {
public:
  Package(unsigned int size) : total(size), cur(0) {
    data = new char[total + 1]();
    data[total] = '\0';
  }

  ~Package() {
    spdlog::info("Package::~Package");
    delete[] data;
  }

  void Clear() {
    ::memset(data, 0, total);
    cur = 0;
  }

  unsigned int total;
  unsigned int cur;
  char *data;
};

class RecvPackage : public Package {
  friend class ServiceSystem;
  friend class LogicPackage;

public:
  RecvPackage(unsigned int packageLen, unsigned int msgID);

  unsigned int id;
};

class SendPackage : public Package {
  friend class ServiceSystem;

public:
  SendPackage(const char *msg, unsigned int maxLen, unsigned int msgID);

  unsigned int id;
};

class LogicPackage {
  friend class ServiceSystem;

public:
  LogicPackage(std::shared_ptr<ChatSession>, std::shared_ptr<RecvPackage>);

  std::shared_ptr<ChatSession> session;
  std::shared_ptr<RecvPackage> recvPackage;
};

}; // namespace protocol