#pragma once

#include "ChatSession.h"
#include "Const.h"
#include "DbGlobal.h"
#include <atomic>
#include <jsoncpp/json/value.h>
#include <memory>
#include <string>
#include <unordered_map>
namespace wim {

static std::unordered_map<long, std::atomic<long>> seqUserMessage;

// ok
class OnlineUser : public Singleton<OnlineUser> {
  friend class Singleton<OnlineUser>;

public:
  ~OnlineUser();
  ChatSession::Ptr GetUserSession(long uid);

  bool MapUser(db::UserInfo::Ptr userInfo, ChatSession::Ptr session);
  void ClearUser(long seq, long uid);
  bool isOnline(long uid);

  void Pong(long uid);

  /*
    @param seq: 序列号
    @param session: 会话
    @param protocolData: 协议响应包
    @param rsp: 响应服务ID
    @param callcount: 递归调用次数
  */
  enum ReWriteType { HeartBeat, Message };
  void onReWrite(ReWriteType type, long seq, long uid,
                 const std::string &protocolData, int rsp, int callcount = 0);

  void cancelAckTimer(long seq);

private:
  OnlineUser();
  std::mutex sessionMutex;
  std::unordered_map<long, ChatSession::Ptr> sessionMap;
  std::map<long, std::shared_ptr<net::steady_timer>> waitAckTimerMap;
};
}; // namespace wim