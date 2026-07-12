#pragma once
#include <functional>
#include <map>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

#include "ChatSession.h"
#include "Const.h"
#include "TcpMessageCodec.h"

namespace wim {

class Service : public Singleton<Service> {
  friend class Singleton<Service>;

 public:
  using HandleType =
      std::function<TcpPacket(ChatSession::Ptr, uint32_t, TcpPacket &)>;

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

TcpPacket OnLogin(ChatSession::Ptr session, uint32_t msgID, TcpPacket &request);

TcpPacket PingHandle(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request);

TcpPacket TextSend(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request);

TcpPacket UploadFile(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request);

TcpPacket FileSend(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request);

TcpPacket pullFriendApplyList(ChatSession::Ptr session, uint32_t msgID,
                              TcpPacket &request);
TcpPacket pullFriendList(ChatSession::Ptr session, uint32_t msgID,
                         TcpPacket &request);
TcpPacket pullSessionMessageList(ChatSession::Ptr session, uint32_t msgID,
                                 TcpPacket &request);
TcpPacket pullMessageList(ChatSession::Ptr session, uint32_t msgID,
                          TcpPacket &request);
TcpPacket UserQuit(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request);

TcpPacket SerachUser(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request);

TcpPacket AckHandle(ChatSession::Ptr session, uint32_t msgID,
                    TcpPacket &request);

};  // namespace wim
