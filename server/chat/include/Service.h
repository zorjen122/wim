#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>

#include "ChatSession.h"
#include "Const.h"
#include "DeliveryService.h"
#include "FileService.h"
#include "FriendService.h"
#include "GroupService.h"
#include "MessageService.h"
#include "RequestContext.h"
#include "TcpMessageCodec.h"
#include "ThreadPool.h"
#include "UserService.h"

namespace wim {

namespace rpc {
class GatewayStreamService;
}

class Service : public Singleton<Service> {
  friend class Singleton<Service>;

 public:
  using HandleType =
      std::function<TcpPacket(ChatSession::Ptr, uint32_t, TcpPacket &)>;

  ~Service();
  void Dispatch(std::shared_ptr<Channel> package);
  bool PostBackgroundTask(ThreadPool::Task task);
  DeliveryService &Deliveries();
  MessageService &Messages();
  void SetGatewayStreamService(rpc::GatewayStreamService *service);
  TcpPacket ExecuteGatewayCommand(uint32_t msgID, int64_t actorUid,
                                  TcpPacket request);

 private:
  enum class TaskType { Light, Heavy };
  struct HandlerEntry {
    HandleType handle;
    TaskType taskType{TaskType::Heavy};
  };

  Service();
  void Init();
  void ProcessChannel(const Channel::Ptr &channel, RequestContext context);
  void HandleRejectedChannel(const Channel::Ptr &channel,
                             ThreadPool::PostStatus status,
                             const RequestContext &context);
  void RegisterHandle(uint32_t msgID, TaskType taskType, HandleType handle);
  TcpPacket Ping(ChatSession::Ptr session, uint32_t msgID, TcpPacket &request);

  DeliveryService deliveryService;
  UserService userService;
  FriendService friendService;
  GroupService groupService;
  MessageService messageService;
  FileService fileService;
  std::unique_ptr<ThreadPool> threadPool;
  std::atomic<uint64_t> lightDispatched{0};
  std::atomic<uint64_t> heavyDispatched{0};
  std::atomic<uint64_t> heavyRejected{0};
  std::chrono::milliseconds requestTimeout{3000};
  std::chrono::milliseconds queueAcquireTimeout{2};
  std::map<uint32_t, HandlerEntry> serviceGroup;
};
};  // namespace wim
