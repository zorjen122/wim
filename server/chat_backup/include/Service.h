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
      std::function<Json::Value(ChatSession::Ptr, uint32_t, Json::Value &)>;

  ~Service();
  void PushService(std::shared_ptr<Channel> package);

private:
  Service();
  void Run();
  void Init();
  void PopHandler();
  void RegisterHandle(uint32_t msgID, HandleType handle);

  std::thread worker;
  std::queue<std::shared_ptr<Channel>> messageQueue;
  std::mutex _mutex;
  std::condition_variable consume;
  bool stopEnable;
  std::map<uint32_t, HandleType> serviceGroup;
};

// 已成功

Json::Value OnLogin(ChatSession::Ptr session, uint32_t msgID,
                    Json::Value &request);

Json::Value PingHandle(ChatSession::Ptr session, uint32_t msgID,
                       Json::Value &request);

Json::Value TextSend(ChatSession::Ptr session, uint32_t msgID,
                     Json::Value &request);

Json::Value UploadFile(ChatSession::Ptr session, uint32_t msgID,
                       Json::Value &request);

Json::Value FileSend(ChatSession::Ptr session, uint32_t msgID,
                     Json::Value &request);

Json::Value pullFriendApplyList(ChatSession::Ptr session, uint32_t msgID,
                                Json::Value &request);
Json::Value pullFriendList(ChatSession::Ptr session, uint32_t msgID,
                           Json::Value &request);
Json::Value pullSessionMessageList(ChatSession::Ptr session, uint32_t msgID,
                                   Json::Value &request);
Json::Value pullMessageList(ChatSession::Ptr session, uint32_t msgID,
                            Json::Value &request);
Json::Value UserQuit(ChatSession::Ptr session, uint32_t msgID,
                     Json::Value &request);

Json::Value SerachUser(ChatSession::Ptr session, uint32_t msgID,
                       Json::Value &request);

Json::Value AckHandle(ChatSession::Ptr session, uint32_t msgID,
                      Json::Value &request);

}; // namespace wim