#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>

#include "ChatSession.h"
#include "Const.h"
#include "TcpMessageCodec.h"
#include "ThreadPool.h"

namespace wim {

class Service : public Singleton<Service> {
  friend class Singleton<Service>;

 public:
  using HandleType =
      std::function<TcpPacket(ChatSession::Ptr, uint32_t, TcpPacket &)>;

  ~Service();
  void Dispatch(std::shared_ptr<Channel> package);
  bool PostBackgroundTask(ThreadPool::Task task);

 private:
  enum class TaskType { Light, Heavy };
  struct HandlerEntry {
    HandleType handle;
    TaskType taskType{TaskType::Heavy};
  };

  Service();
  void Init();
  void ProcessChannel(const Channel::Ptr &channel);
  void HandleRejectedChannel(const Channel::Ptr &channel);
  void RegisterHandle(uint32_t msgID, TaskType taskType, HandleType handle);

  std::unique_ptr<ThreadPool> threadPool;
  std::atomic<uint64_t> lightDispatched{0};
  std::atomic<uint64_t> heavyDispatched{0};
  std::atomic<uint64_t> heavyRejected{0};
  std::map<uint32_t, HandlerEntry> serviceGroup;
};

// 已成功

TcpPacket OnLogin(ChatSession::Ptr session, uint32_t msgID, TcpPacket &request);

TcpPacket PingHandle(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request);

TcpPacket AckHandle(ChatSession::Ptr session, uint32_t msgID,
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
};  // namespace wim
