#pragma once
#include "json/value.h"
#include <functional>
#include <json/json.h>
#include <map>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

#include "ChatSession.h"
#include "Const.h"
class Service : public Singleton<Service> {
  friend class Singleton<Service>;

public:
  using HandleType = std::function<void(std::shared_ptr<ChatSession>,
                                        unsigned int, Json::Value)>;
  using PackageType = Json::Value;

  ~Service();
  void PushService(std::shared_ptr<ChatSession::RequestPackage> package);

private:
  Service();
  void Run();
  void Init();
  void PopHandler();

  std::thread worker;
  std::queue<std::shared_ptr<ChatSession::RequestPackage>> messageQueue;
  std::mutex _mutex;
  std::condition_variable consume;
  bool isStop;
  std::map<unsigned int, HandleType> serviceGroup;
};

namespace util {
static std::atomic<size_t> seqGenerator(1);
static std::unordered_map<
    size_t, std::unordered_map<size_t, std::shared_ptr<net::steady_timer>>>
    retansfTimerMap;

static std::unordered_map<size_t, std::unordered_map<size_t, size_t>>
    retansfCountMap;
static std::unordered_map<std::shared_ptr<ChatSession>,
                          std::shared_ptr<ChatSession>>
    sChannel;

static void clearRetransfTimer(size_t seq, size_t member) {
  auto &timer = retansfTimerMap[seq][member];
  if (timer == nullptr) {
    spdlog::error(
        "[clearRetransfTimer] timer is nullptr | seq [{}], member [{}]", seq,
        member);
    return;
  }
  timer->cancel();
  spdlog::info("[clearRetansfTimer]");
}

static size_t SeqAllocate() {
  ++seqGenerator;
  return static_cast<size_t>(seqGenerator.load());
}
} // namespace util

void ClearChannel(size_t uid, std::shared_ptr<ChatSession> session);

void PingHandle(std::shared_ptr<ChatSession> session, unsigned int msgID,
                const Service::PackageType &msgData);

void Login(std::shared_ptr<ChatSession> session, unsigned int msgID,
           const Service::PackageType &msgData);

void ReLogin(int uid, std::shared_ptr<ChatSession> oldSession,
             std::shared_ptr<ChatSession> newSession);

void UserQuit(std::shared_ptr<ChatSession> session, unsigned int msgID,
              const Service::PackageType &msgData);

void Pong(int uid, std::shared_ptr<ChatSession> session);

int PullText(size_t seq, int from, int to, const std::string &msg);

int PushText(std::shared_ptr<ChatSession> toSession, size_t seq, int from,
             int to, const std::string &msg, int msgID = ID_TEXT_SEND_RSP);

bool SaveService(size_t seq, int from, int to, std::string msg);
bool SaveServiceDB(size_t seq, int from, int to, const std::string &msg);

int OnRewriteTimer(std::shared_ptr<ChatSession> session, size_t seq,
                   const std::string &rsp, unsigned int rspID,
                   unsigned int member, unsigned int timewait = 5,
                   unsigned int maxRewrite = 3);

void AckHandle(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const Service::PackageType &msgData);

void UserSearch(std::shared_ptr<ChatSession> session, unsigned int msgID,
                const Service::PackageType &msgData);

void TextSend(std::shared_ptr<ChatSession> session, unsigned int msgID,
              const Service::PackageType &msgData);
