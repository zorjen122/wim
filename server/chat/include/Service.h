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
      std::function<void(ChatSession::Ptr, unsigned int, const Json::Value &)>;

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

namespace util {
static std::unordered_map<
    size_t, std::unordered_map<size_t, std::shared_ptr<net::steady_timer>>>
    retansfTimerMap;

static std::unordered_map<size_t, std::unordered_map<size_t, size_t>>
    retansfCountMap;
static std::unordered_map<ChatSession::Ptr, ChatSession::Ptr> sChannel;

static void clearRetransfTimer(size_t seq, size_t member) {
  auto &timer = retansfTimerMap[seq][member];
  if (timer == nullptr) {
    spdlog::info(
        "[clearRetransfTimer] timer is nullptr | seq [{}], member [{}]", seq,
        member);
    return;
  }
  timer->cancel();
  spdlog::info("[clearRetansfTimer]");
}

} // namespace util

// 已成功

void OnLogin(ChatSession::Ptr session, unsigned int msgID,
             const Json::Value &request);

// 待测试

void pullFriendApplyList(ChatSession::Ptr session, unsigned int msgID,
                         const Json::Value &request);
void pullFriendList(ChatSession::Ptr session, unsigned int msgID,
                    const Json::Value &request);
void pullMessageList(ChatSession::Ptr session, unsigned int msgID,
                     const Json::Value &request);

void ClearChannel(size_t uid, ChatSession::Ptr session);

void PingHandle(ChatSession::Ptr session, unsigned int msgID,
                const Json::Value &request);

void ReLogin(long uid, ChatSession::Ptr oldSession,
             ChatSession::Ptr newSession);

void UserQuit(ChatSession::Ptr session, unsigned int msgID,
              const Json::Value &request);

void Pong(long uid, ChatSession::Ptr session);

int PullText(size_t seq, int from, int to, const std::string &msg);

int PushText(ChatSession::Ptr toSession, size_t seq, int from, int to,
             const std::string &msg, int msgID = ID_TEXT_SEND_RSP);

bool SaveService(size_t seq, int from, int to, std::string msg);
bool SaveServiceDB(size_t seq, int from, int to, const std::string &msg);

int OnRewriteTimer(ChatSession::Ptr session, size_t seq, const std::string &rsp,
                   unsigned int rspID, unsigned int member,
                   unsigned int timewait = 5, unsigned int maxRewrite = 3);

void AckHandle(ChatSession::Ptr session, unsigned int msgID,
               const Json::Value &request);

void UserSearch(ChatSession::Ptr session, unsigned int msgID,
                const Json::Value &request);

void TextSend(ChatSession::Ptr session, unsigned int msgID,
              const Json::Value &request);
}; // namespace wim