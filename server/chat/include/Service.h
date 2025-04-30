#pragma once
#include <functional>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <map>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

#include "ChatSession.h"
#include "Const.h"

namespace wim {

class Service : public Singleton<Service> {
  friend class Singleton<Service>;

public:
  using HandleType =
      std::function<Json::Value(ChatSession::Ptr, unsigned int, Json::Value &)>;

  ~Service();
  void PushService(std::shared_ptr<NetworkMessage> package);

private:
  Service();
  void Run();
  void Init();
  void PopHandler();

  std::thread worker;
  std::queue<std::shared_ptr<NetworkMessage>> messageQueue;
  std::mutex _mutex;
  std::condition_variable consume;
  bool isStop;
  std::map<unsigned int, HandleType> serviceGroup;
};

// 已成功

Json::Value OnLogin(ChatSession::Ptr session, unsigned int msgID,
                    Json::Value &request);

// 待测试

Json::Value pullFriendApplyList(ChatSession::Ptr session, unsigned int msgID,
                                Json::Value &request);
Json::Value pullFriendList(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request);
Json::Value pullMessageList(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request);

Json::Value PingHandle(ChatSession::Ptr session, unsigned int msgID,
                       Json::Value &request);

Json::Value TextSend(ChatSession::Ptr session, unsigned int msgID,
                     Json::Value &request);

Json::Value UserQuit(ChatSession::Ptr session, unsigned int msgID,
                     Json::Value &request);

Json::Value SerachUser(ChatSession::Ptr session, unsigned int msgID,
                       Json::Value &request);

// 待实现

Json::Value AckHandle(ChatSession::Ptr session, unsigned int msgID,
                      Json::Value &request);

}; // namespace wim