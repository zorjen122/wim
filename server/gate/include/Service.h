#pragma once
#include <functional>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <map>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

#include "Const.h"
#include "ImSession.h"
#include "Session.h"

namespace wim {

class Service : public Singleton<Service> {
  friend class Singleton<Service>;

public:
  using HandleType = std::function<int(Json::Value &)>;

  ~Service();
  void PushService(Session::ptr channel);

private:
  Service();
  void Run();
  void Init();
  void RegisterHandle(uint32_t msgID, HandleType handle);
  void RouteForward(Session::ptr channel);

  std::thread worker;
  std::queue<Session::ptr> messageQueue;
  std::mutex _mutex;
  std::condition_variable consume;
  bool stopEnable;
  std::map<uint32_t, HandleType> forwardServiceGroup;
  ImSessionManager::Ptr imSessionManager;
};

}; // namespace wim