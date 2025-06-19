#pragma once

#include "ChatSession.h"
#include "Const.h"
#include "DbGlobal.h"
#include <jsoncpp/json/value.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace wim {

// ok
class OnlineUser : public Singleton<OnlineUser> {
  friend class Singleton<OnlineUser>;

public:
  ~OnlineUser();
  ChatSession::ptr GetUserSession(long uid);

  bool MapUser(db::UserInfo::Ptr userInfo, ChatSession::ptr session);
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
  std::string getReWriteString(ReWriteType type);

  void onReWrite(ReWriteType type, long seq, long uid, std::string packet,
                 int rsp, int callcount = 0);

  void cancelAckTimer(long seq, long uid);

private:
  OnlineUser();
  std::mutex sessionMutex;
  std::unordered_map<long, ChatSession::ptr> sessionMap;

  /*
    1、此字段是通用的，作用于好友、群聊、心跳。
    2、对于一对一好友通讯，表示为：<seq, uid->timer>。
    3、对于群聊通讯，表示为：<seq, <member1 -> timer, member2->timer, ...,
    memberN->timer>>； 心跳时，表示为：<uid, uid->timer>。
  */
  std::unordered_map<long,
                     std::unordered_map<long, std::shared_ptr<steady_timer>>>
      waitAckTimerMap;
};
}; // namespace wim