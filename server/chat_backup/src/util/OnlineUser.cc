#include "OnlineUser.h"
#include "DbGlobal.h"
#include "Logger.h"
#include "Mysql.h"
#include "Redis.h"

namespace wim {

OnlineUser::~OnlineUser() { sessionMap.clear(); }
// userId:machineId
// machineId -> machine IP

ChatSession::Ptr OnlineUser::GetUserSession(long uid) {
  std::lock_guard<std::mutex> lock(sessionMutex);
  auto iter = sessionMap.find(uid);
  if (iter == sessionMap.end()) {
    return nullptr;
  }

  return iter->second;
}

bool OnlineUser::MapUser(db::UserInfo::Ptr userInfo, ChatSession::Ptr session) {
  YAML::Node config = Configer::getNode("server");
  std::string selfMachineId = config["self"]["name"].as<std::string>();

  std::lock_guard<std::mutex> lock(sessionMutex);

  bool status =
      db::RedisDao::GetInstance()->setOnlineUserInfo(userInfo, selfMachineId);
  if (status == false) {
    LOG_WARN(businessLogger, "setOnlineUserInfo failed, uid:{}, machineId:{}",
             userInfo->uid, selfMachineId);
    return false;
  }

  sessionMap[userInfo->uid] = session;
  return true;
}
void OnlineUser::cancelAckTimer(long seq, long uid) {
  if (waitAckTimerMap.find(seq) != waitAckTimerMap.end())
    waitAckTimerMap[seq][uid]->cancel();
}

void OnlineUser::ClearUser(long seq, long uid) {
  std::lock_guard<std::mutex> lock(sessionMutex);

  db::RedisDao::GetInstance()->delOnlineUserInfo(uid);

  if (waitAckTimerMap.find(seq) != waitAckTimerMap.end())
    waitAckTimerMap.erase(seq);

  sessionMap[uid]->ClearSession();
  if (sessionMap.find(uid) != sessionMap.end())
    sessionMap.erase(uid);
}

OnlineUser::OnlineUser() {}

bool OnlineUser::isOnline(long uid) { return GetUserSession(uid) != nullptr; }

std::string OnlineUser::getReWriteString(ReWriteType type) {
  switch (type) {
  case ReWriteType::HeartBeat:
    return "heartbeat";
  case ReWriteType::Message:
    return "message";
  default:
    return "";
  }
}

void OnlineUser::onReWrite(OnlineUser::ReWriteType type, long seq, long uid,
                           std::string packet, int rsp, int callcount) {

  sessionMap[uid]->Send(packet, rsp);

  // 此负载到用户会话上下文
  auto waitAckTimer =
      std::make_shared<net::steady_timer>(sessionMap[uid]->GetIoc());
  waitAckTimerMap[seq][uid] = waitAckTimer;
  waitAckTimer->expires_after(std::chrono::seconds(5));

  waitAckTimer->async_wait([this, type, seq, uid, packet, rsp,
                            callcount](const boost::system::error_code &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      LOG_DEBUG(wim::businessLogger,
                "重传定时器取消, 序列号：{}, 重传次数 {}, 类型：{}", uid, seq,
                callcount, getReWriteString(type));
      return;
    } else {
      static const int max_timeout_count = 3;
      if (callcount + 1 >= max_timeout_count) {

        // 对于心跳类型重传，seq为用户Id
        if (type == ReWriteType::HeartBeat) {
          LOG_INFO(
              businessLogger,
              "重传心跳包响应超时，用户ID: {}, 序列号:{}, 重传次数{}, 类型：{}",
              uid, seq, callcount, getReWriteString(type));
          ClearUser(uid, uid);
        } else if (type == ReWriteType::Message) {
          LOG_INFO(
              businessLogger,
              "重传消息响应超时, 用户ID：{}，序列号：{}, 重传次数 {}, 类型：{}",
              uid, seq, callcount, getReWriteString(type));
          ClearUser(seq, uid);
        }

        return;
      }

      /*
       当用户主动断开连接时，接入层会主动关闭连接，由于此时session已被该域引用计数，故而不会释放，
       此时可查看连接状态，以选择是否重发
       */
      if (sessionMap[uid]->IsConnected()) {
        onReWrite(type, seq, uid, packet, rsp, callcount + 1);
      } else {
        LOG_DEBUG(wim::businessLogger,
                  "session closed, 序列号：{}, 重传次数 {}, type: {}, session "
                  "refcount: {}",
                  seq, callcount, short(type), sessionMap[uid].use_count());
        // 对于心跳类型重传，seq为用户Id
        if (type == ReWriteType::HeartBeat) {
          ClearUser(uid, uid);
        } else if (type == ReWriteType::Message) {
          ClearUser(seq, uid);
          // 暂无其他处理
        }
      }
    }
  });
}

void OnlineUser::Pong(long uid) {

  cancelAckTimer(uid, uid);

  Json::Value pong;
  pong["uid"] = Json::Value::Int64(uid);
  pong["error"] = ErrorCodes::Success;

  sessionMap[uid]->Send(pong.toStyledString(), ID_PING_RSP);

  // 使用用户Id作为序列号，因心跳仅监视状态
  onReWrite(ReWriteType::HeartBeat, uid, uid, pong.toStyledString(),
            ID_PING_RSP);

  LOG_DEBUG(wim::businessLogger, "Pong, uid: {}", uid);
}

}; // namespace wim