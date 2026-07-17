#include "Service.h"

#include "ChatSession.h"
#include "Const.h"
#include "Logger.h"
#include "Metrics.h"
#include "RequestContext.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace wim {
namespace {

std::size_t ResolveServiceWorkerCount() {
  if (const char *value = std::getenv("WIM_CHAT_SERVICE_WORKERS")) {
    int configured = std::atoi(value);
    if (configured > 0) {
      return static_cast<std::size_t>(configured);
    }
  }

  std::size_t hardware = std::thread::hardware_concurrency();
  if (hardware == 0) {
    hardware = 4;
  }
  return std::clamp<std::size_t>(hardware / 2, 2, 8);
}

std::chrono::milliseconds ResolvePositiveMilliseconds(const char *name,
                                                      int fallback) {
  if (const char *value = std::getenv(name)) {
    int configured = std::atoi(value);
    if (configured > 0) {
      return std::chrono::milliseconds(configured);
    }
  }
  return std::chrono::milliseconds(fallback);
}

}  // namespace

Service::Service()
    : friendService(deliveryService),
      groupService(deliveryService),
      messageService(deliveryService) {
  Init();
  requestTimeout =
      ResolvePositiveMilliseconds("WIM_CHAT_REQUEST_TIMEOUT_MS", 3000);
  queueAcquireTimeout =
      ResolvePositiveMilliseconds("WIM_CHAT_QUEUE_ACQUIRE_MS", 2);
  threadPool =
      std::make_unique<ThreadPool>("chat-biz", ResolveServiceWorkerCount());
}

Service::~Service() {
  if (threadPool) {
    auto stats = threadPool->GetStats();
    LOG_INFO(businessLogger,
             "chat service dispatcher stop, workers: {}, queued: {}, light "
             "direct: {}, heavy dispatched/rejected: {}/{}, thread pool "
             "submitted/completed/rejected/acquire-timeout/expired: "
             "{}/{}/{}/{}/{}, requests started/succeeded/failed/expired: "
             "{}/{}/{}/{}",
             stats.workerCount, stats.queueSize,
             lightDispatched.load(std::memory_order_relaxed),
             heavyDispatched.load(std::memory_order_relaxed),
             heavyRejected.load(std::memory_order_relaxed), stats.submitted,
             stats.completed, stats.rejected, stats.acquireTimeouts,
             stats.expired, Metrics::Get(Metric::RequestsStarted),
             Metrics::Get(Metric::RequestsSucceeded),
             Metrics::Get(Metric::RequestsFailed),
             Metrics::Get(Metric::RequestsExpired));
    threadPool->Stop();
  }
}

void Service::RegisterHandle(uint32_t msgID, TaskType taskType,
                             HandleType handle) {
  if (serviceGroup.find(msgID) != serviceGroup.end()) {
    LOG_WARN(businessLogger, "该服务已注册,msgID: {}", msgID);
    return;
  }
  serviceGroup[msgID] = HandlerEntry{std::move(handle), taskType};
}

void Service::Init() {
  // 状态
  RegisterHandle(ID_LOGIN_INIT_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return userService.Login(session, msgID, request);
                 });
  RegisterHandle(ID_USER_QUIT_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return userService.Quit(session, msgID, request);
                 });
  RegisterHandle(ID_PING_REQ, TaskType::Light,
                 [this](auto session, auto msgID, auto &request) {
                   return Ping(session, msgID, request);
                 });
  RegisterHandle(ID_ACK, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return messageService.Ack(msgID, request);
                 });

  // 消息与文件
  RegisterHandle(ID_TEXT_SEND_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return messageService.SendText(msgID, request);
                 });
  RegisterHandle(ID_FILE_UPLOAD_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return fileService.Upload(msgID, request);
                 });

  // 好友
  RegisterHandle(ID_NOTIFY_ADD_FRIEND_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return friendService.NotifyAddFriend(msgID, request);
                 });
  RegisterHandle(ID_REPLY_ADD_FRIEND_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return friendService.ReplyAddFriend(msgID, request);
                 });
  RegisterHandle(ID_PULL_FRIEND_LIST_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return friendService.PullFriendList(msgID, request);
                 });
  RegisterHandle(ID_PULL_FRIEND_APPLY_LIST_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return friendService.PullFriendApplyList(msgID, request);
                 });

  // 消息拉取
  RegisterHandle(ID_PULL_SESSION_MESSAGE_LIST_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return messageService.PullSessionMessages(msgID, request);
                 });
  RegisterHandle(ID_PULL_MESSAGE_LIST_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return messageService.PullMessages(msgID, request);
                 });

  // 群聊
  RegisterHandle(ID_GROUP_CREATE_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return groupService.Create(msgID, request);
                 });
  RegisterHandle(ID_GROUP_NOTIFY_JOIN_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return groupService.NotifyJoin(msgID, request);
                 });
  RegisterHandle(ID_GROUP_REPLY_JOIN_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return groupService.ReplyJoin(msgID, request);
                 });

  /* 待实现 */
  RegisterHandle(ID_GROUP_TEXT_SEND_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return messageService.SendGroupText(msgID, request);
                 });
  RegisterHandle(ID_FILE_SEND_REQ, TaskType::Heavy,
                 [this](auto session, auto msgID, auto &request) {
                   return messageService.SendFile(msgID, request);
                 });
}

void Service::Dispatch(std::shared_ptr<Channel> msg) {
  auto channel = std::move(msg);
  uint32_t id = channel->protocolData->id;
  auto context = RequestContext::WithTimeout(
      RequestContext::NextRequestId(), getServiceIdString(id),
      RequestSource::Tcp, channel->contextSession->GetUserId(), requestTimeout);
  Metrics::Increment(Metric::RequestsStarted);
  auto handleCaller = serviceGroup.find(id);
  TaskType taskType = handleCaller == serviceGroup.end()
                          ? TaskType::Light
                          : handleCaller->second.taskType;

  if (taskType == TaskType::Light) {
    lightDispatched.fetch_add(1, std::memory_order_relaxed);
    ProcessChannel(channel, std::move(context));
    return;
  }

  // I/O 线程只给入队一个很短预算，避免业务池拥塞反向阻塞网络循环。
  auto acquireDeadline = std::min(
      context.deadline, RequestContext::Clock::now() + queueAcquireTimeout);
  auto status = threadPool->PostUntil(
      [this, channel, context]() mutable {
        ProcessChannel(channel, std::move(context));
      },
      acquireDeadline, context.deadline);
  if (status != ThreadPool::PostStatus::Accepted) {
    heavyRejected.fetch_add(1, std::memory_order_relaxed);
    Metrics::Increment(Metric::RequestsFailed);
    HandleRejectedChannel(channel, status, context);
    return;
  }

  heavyDispatched.fetch_add(1, std::memory_order_relaxed);
}

void Service::ProcessChannel(const Channel::Ptr &channel,
                             RequestContext context) {
  RequestContextScope contextScope(context);
  uint32_t id = channel->protocolData->id;
  const char *data = channel->protocolData->data;
  uint32_t dataSize = channel->protocolData->getDataSize();
  int responseId = __getServiceResponseId(ServiceID(id));

  if (context.Expired()) {
    Metrics::Increment(Metric::RequestsExpired);
    Metrics::Increment(Metric::RequestsFailed);
    LOG_WARN(
        businessLogger,
        "请求在线程池排队期间过期, requestId: {}, operation: {}, actor: {}",
        context.requestId, context.operation, context.actor);
    if (id != ID_ACK) {
      TcpPacket rsp = MakeErrorPacket(ErrorCodes::DeadlineExceeded,
                                      "request deadline exceeded in queue");
      channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
    }
    return;
  }

  auto handleCaller = serviceGroup.find(id);
  if (handleCaller == serviceGroup.end()) {
    LOG_INFO(wim::businessLogger, "没有这样的服务，ID： {}, ", id);

    TcpPacket rsp = MakeErrorPacket(ErrorCodes::NotFound);
    channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
    Metrics::Increment(Metric::RequestsFailed);
    return;
  }

  std::string msgData{data, dataSize};
  TcpPacket request, response;
  std::string requestIdMessage = getServiceIdString(id),
              responseIdMessage = getServiceIdString(responseId);

  bool parserSuccess = ParseTcpPacket(msgData, request);
  if (!parserSuccess) {
    LOG_WARN(wim::businessLogger, "消息解析失败, 请求服务: {}, 消息长度: {}",
             requestIdMessage, dataSize);
    TcpPacket rsp = MakeErrorPacket(ErrorCodes::JsonParser);
    channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
    Metrics::Increment(Metric::RequestsFailed);
    return;
  }
  if (request.has_request_timeout_ms() && request.request_timeout_ms() > 0) {
    // 客户端只能缩短服务端预算，不能通过超大 timeout 延长请求生命周期。
    auto clientDeadline = context.startedAt + std::chrono::milliseconds(
                                                  request.request_timeout_ms());
    context.deadline = std::min(context.deadline, clientDeadline);
  }
  if (request.has_request_id() && !request.request_id().empty()) {
    context.requestId = request.request_id();
  }
  if (context.Expired()) {
    Metrics::Increment(Metric::RequestsExpired);
    Metrics::Increment(Metric::RequestsFailed);
    LOG_WARN(businessLogger,
             "请求解析后已超过客户端截止时间, requestId: {}, operation: {}",
             context.requestId, context.operation);
    if (id != ID_ACK) {
      TcpPacket rsp = MakeErrorPacket(ErrorCodes::DeadlineExceeded,
                                      "client request deadline exceeded");
      channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
    }
    return;
  }
  // TCP 信任边界：登录以外的业务请求统一从 session 注入可信 actor。
  if (id != ID_LOGIN_INIT_REQ && !channel->contextSession->IsAuthenticated()) {
    TcpPacket rsp = MakeErrorPacket(ErrorCodes::AuthenticationRequired,
                                    "chat session is not authenticated");
    channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
    Metrics::Increment(Metric::RequestsFailed);
    return;
  }
  if (id != ID_LOGIN_INIT_REQ) {
    request.set_uid(channel->contextSession->GetUserId());
  }
  context.actor = request.uid();
  if (!request.has_request_id() && request.has_seq()) {
    context.requestId = std::to_string(context.actor) + ":" +
                        std::to_string(id) + ":" +
                        std::to_string(request.seq());
  }
  LOG_DEBUG(businessLogger,
            "请求上下文已建立, requestId: {}, operation: {}, actor: {}, "
            "remainingMs: {}",
            context.requestId, context.operation, context.actor,
            context.Remaining().count());
  if (id == ID_LOGIN_INIT_REQ) {
    LOG_DEBUG(wim::businessLogger, "解析成功，请求服务: {}, uid: {}",
              requestIdMessage, request.uid());
  } else {
    LOG_DEBUG(wim::businessLogger, "解析成功，请求服务: {}, 请求数据: {}",
              requestIdMessage, TcpPacketDebugString(request));
  }

  response = handleCaller->second.handle(channel->contextSession, id, request);
  if (id == ID_ACK) {
    LOG_DEBUG(wim::businessLogger, "ACK已处理，不发送响应包: {}",
              TcpPacketDebugString(response));
    Metrics::Increment(TcpPacketError(response) == ErrorCodes::Success
                           ? Metric::RequestsSucceeded
                           : Metric::RequestsFailed);
    return;
  }
  auto ret = SerializeTcpPacket(response);
  channel->contextSession->Send(ret, responseId);

  Metrics::Increment(TcpPacketError(response) == ErrorCodes::Success
                         ? Metric::RequestsSucceeded
                         : Metric::RequestsFailed);

  LOG_DEBUG(wim::businessLogger, "响应服务: {}, 响应数据: {}",
            responseIdMessage, TcpPacketDebugString(response));
}

void Service::HandleRejectedChannel(const Channel::Ptr &channel,
                                    ThreadPool::PostStatus status,
                                    const RequestContext &context) {
  uint32_t id = channel->protocolData->id;
  LOG_WARN(
      businessLogger,
      "业务请求入队失败, requestId: {}, operation: {}, actor: {}, status: {}",
      context.requestId, context.operation, context.actor,
      status == ThreadPool::PostStatus::TimedOut ? "timeout" : "stopped");
  if (id == ID_ACK) {
    LOG_WARN(wim::businessLogger, "ACK任务被拒绝，当前业务线程池已满");
    return;
  }

  int responseId = __getServiceResponseId(ServiceID(id));
  int error = status == ThreadPool::PostStatus::TimedOut
                  ? ErrorCodes::ResourceExhausted
                  : ErrorCodes::DependencyUnavailable;
  TcpPacket rsp = MakeErrorPacket(error, "chat service queue unavailable");
  channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
}

bool Service::PostBackgroundTask(ThreadPool::Task task) {
  if (!threadPool) {
    return false;
  }
  return threadPool->Post(std::move(task));
}

DeliveryService &Service::Deliveries() {
  return deliveryService;
}

MessageService &Service::Messages() {
  return messageService;
}

void Service::SetGatewayStreamService(rpc::GatewayStreamService *service) {
  deliveryService.SetGatewayStreamService(service);
}

TcpPacket Service::ExecuteGatewayCommand(uint32_t msgID, int64_t actorUid,
                                         TcpPacket request) {
  if (actorUid <= 0 || msgID == ID_LOGIN_INIT_REQ || msgID == ID_PING_REQ ||
      msgID == ID_USER_QUIT_REQ) {
    return MakeErrorPacket(ErrorCodes::AuthenticationRequired,
                           "connection control command belongs to gateway");
  }
  auto handle = serviceGroup.find(msgID);
  if (handle == serviceGroup.end())
    return MakeErrorPacket(ErrorCodes::NotFound);
  request.set_uid(actorUid);
  return handle->second.handle(nullptr, msgID, request);
}

TcpPacket Service::Ping(ChatSession::Ptr session, uint32_t msgID,
                        TcpPacket &request) {
  TcpPacket rsp;
  rsp.set_uid(request.uid());
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

}  // namespace wim
