#include "Group.h"

#include "Const.h"
#include "DbGlobal.h"
#include "ImRpc.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"
#include <cstdint>
#include <jsoncpp/json/value.h>
#include <spdlog/spdlog.h>
namespace wim {

Json::Value GroupCreate(ChatSession::ptr session, unsigned int msgID,
                        Json::Value &request) {

  Json::Value rsp;

  int ret{};

  // gid should by server generate, todo...
  int64_t uid = request["uid"].asInt64();
  std::string groupName = request["groupName"].asString();

  int64_t gid = db::RedisDao::GetInstance()->generateMsgId();
  rsp["gid"] = Json::Value::Int64(gid);

  int64_t sessionKey = db::RedisDao::GetInstance()->generateSessionId();
  std::string createTime = getCurrentDateTime();
  db::GroupManager::Ptr group(
      new db::GroupManager(gid, sessionKey, groupName, createTime));

  ret = db::MysqlDao::GetInstance()->insertGroup(group);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  db::GroupMember::Ptr member(
      new db::GroupMember(gid, uid, db::GroupMember::Role::Master, createTime));
  ret = db::MysqlDao::GetInstance()->insertGroupMember(member);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  rsp["error"] = ErrorCodes::Success;
  rsp["sessionKey"] = Json::Value::Int64(sessionKey);
  return rsp;
}

int NotifyMemberJoin(int64_t uid, int64_t gid,
                     const std::string &requestMessage) {
  db::GroupMember::MemberList managerList =
      db::MysqlDao::GetInstance()->getGroupRoleMemberList(
          gid, db::GroupMember::Role::Manager);
  for (auto manager : managerList) {
    Json::Value notifyRequest;
    bool isLocalMachineOnline =
        OnlineUser::GetInstance()->isOnline(manager->uid);

    Json::Value userInfo =
        db::RedisDao::GetInstance()->getOnlineUserInfoObject(manager->uid);
    bool isOtherMachineOnline =
        isLocalMachineOnline == false && !userInfo.empty();
    if (isOtherMachineOnline) {
      // rpc
      LOG_INFO(businessLogger, "RPC转发通知群成员加入请求，待实现");
      continue;
    }
    if (isLocalMachineOnline) {
      auto user = OnlineUser::GetInstance()->GetUserSession(manager->uid);
      int64_t serverSeq = db::RedisDao::GetInstance()->generateMsgId();
      notifyRequest["uid"] = Json::Value::Int64(uid);
      notifyRequest["gid"] = Json::Value::Int64(gid);
      notifyRequest["type"]; // todo
      notifyRequest["content"] = requestMessage;
      notifyRequest["seq"] = Json::Value::Int64(serverSeq);
      notifyRequest["error"] = ErrorCodes::Success;

      // 同好友申请一样，通知REQ发送到客户端则表示通知接收者
      OnlineUser::GetInstance()->onReWrite(
          OnlineUser::ReWriteType::Message, serverSeq, manager->uid,
          notifyRequest.toStyledString(), ID_GROUP_NOTIFY_JOIN_REQ);
    }
  }
  return 0;
}

Json::Value GroupNotifyJoin(ChatSession::ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;

  int64_t uid = request["uid"].asInt64();
  int64_t gid = request["gid"].asInt64();
  std::string requestMessage = request["requestMessage"].asString();
  // 搜索群聊时存在，但加入群聊时未必存在，此处仍有一致性问题，后续待拟
  int ret = db::MysqlDao::GetInstance()->hasGroup(gid);
  if (ret == 0) {
    rsp["error"] = ErrorCodes::GroupNotExists;
    rsp["message"] = "群组不存在";
    return rsp;
  } else if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  // 暂用好友相关表，消息将通知所有群管理
  std::string createTime = getCurrentDateTime();
  db::GroupApply::Ptr apply(new db::GroupApply(
      uid, 0, gid, db::GroupApply::Type::Add, db::GroupApply::Status::Wait,
      requestMessage, createTime));

  ret = db::MysqlDao::GetInstance()->insertGroupApply(apply);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  ret = NotifyMemberJoin(uid, gid, requestMessage);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::GroupNotExists;
    rsp["message"] = "通知群成员失败";
    return rsp;
  }

  rsp["error"] = ErrorCodes::Success;
  rsp["message"] = "通知群成员成功";

  return rsp;
}

Json::Value GroupPullNotify(ChatSession::ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;
  rsp["gid"] = request["gid"];

  int64_t gid = request["gid"].asInt64();

  int ret = db::MysqlDao::GetInstance()->hasGroup(gid);
  if (ret == false) {
    rsp["error"] = ErrorCodes::GroupNotExists;
    rsp["message"] = "群组已不存在";
  }

  db::FriendApply::FriendApplyGroup applyList =
      db::MysqlDao::GetInstance()->getFriendApplyList(gid);
  if (applyList == nullptr) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  for (auto apply : *applyList) {
    Json::Value notifyInfo;
    notifyInfo["requestor"] = Json::Value::Int64(apply->to);
    rsp["applyList"].append(notifyInfo);
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
}
int NotifyMemberReply(int64_t gid, int64_t managerUid, int64_t requestorUid,
                      bool accept) {
  db::GroupMember::MemberList memberList =
      db::MysqlDao::GetInstance()->getGroupMemberList(gid);
  for (auto member : memberList) {
    bool isLocalMachineOnline =
        OnlineUser::GetInstance()->isOnline(member->uid);

    Json::Value userInfo =
        db::RedisDao::GetInstance()->getOnlineUserInfoObject(member->uid);
    bool isOtherMachineOnline =
        isLocalMachineOnline == false && !userInfo.empty();
    if (isOtherMachineOnline) {
      // rpc
      LOG_INFO(businessLogger, "RPC转发通知群成员加入请求，待实现");
      continue;
    }
    if (isLocalMachineOnline) {
      Json::Value notifyRequest;
      auto user = OnlineUser::GetInstance()->GetUserSession(member->uid);
      int64_t serverSeq = db::RedisDao::GetInstance()->generateMsgId();
      notifyRequest["requestorUid"] = Json::Value::Int64(requestorUid);
      notifyRequest["replyorUid"] = Json::Value::Int64(managerUid);
      notifyRequest["gid"] = Json::Value::Int64(gid);
      notifyRequest["accept"] = accept;
      notifyRequest["type"];
      notifyRequest["content"];
      notifyRequest["seq"] = Json::Value::Int64(serverSeq);

      // 同好友申请一样，通知REQ发送到客户端则表示通知接收者
      OnlineUser::GetInstance()->onReWrite(
          OnlineUser::ReWriteType::Message, serverSeq, member->uid,
          notifyRequest.toStyledString(), ID_GROUP_REPLY_JOIN_REQ);
    }
  }

  return 0;
}
Json::Value GroupReplyJoin(ChatSession::ptr session, unsigned int msgID,
                           Json::Value &request) {
  Json::Value rsp;
  rsp["gid"] = request["gid"];

  int64_t gid = request["gid"].asInt64();
  int64_t managerUid = request["managerUid"].asInt64();
  int64_t requestorUid = request["requestorUid"].asInt64();
  bool accept = request["accept"].asBool();

  // 搜索群聊时存在，但加入群聊时未必存在
  int ret = db::MysqlDao::GetInstance()->hasGroup(gid);
  if (ret == 0) {
    rsp["error"] = ErrorCodes::GroupNotExists;
    rsp["message"] = "群组不存在";
    return rsp;
  }

  // 暂用好友相关表，消息将通知所有群管理
  std::string createTime = getCurrentDateTime();
  db::GroupApply::Ptr apply(new db::GroupApply(
      requestorUid, managerUid, gid, db::GroupApply::Type::Add,
      db::GroupApply::Status::Agree, "", createTime));

  // 在之前应有检查是否已经同意过，待实现
  ret = db::MysqlDao::GetInstance()->updateGroupApply(apply);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  db::GroupMember::Ptr member(new db::GroupMember(
      gid, requestorUid, db::GroupMember::Role::Member, createTime));
  ret = db::MysqlDao::GetInstance()->insertGroupMember(member);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
    return rsp;
  }

  // 此时请求者已成为群成员，通知所有群成员中包含请求者
  ret = NotifyMemberReply(gid, managerUid, requestorUid, accept);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::GroupReplyFailed;
    rsp["message"] = "通知群成员失败";
    return rsp;
  }

  if (!accept) {
    rsp["error"] = ErrorCodes::Success;
    rsp["message"] = "拒绝该用户加入群组请求";
    return rsp;
  }

  rsp["error"] = ErrorCodes::Success;
  rsp["message"] = "同意加入群组";
  return rsp;
}

Json::Value GroupQuit(ChatSession::ptr session, unsigned int msgID,
                      Json::Value &request) {
  Json::Value rsp;
  return rsp;
}

/*
  1.检查群聊
  2.建立<seq, [member1, member2,
  ...]>映射，一条消息对应一个seq，一个seq对应多个成员
  3.转发到群聊所分配的会话服务器，由其生成seq，得到所有成员并按在线离线状态分发消息
*/
Json::Value GroupTextSend(ChatSession::ptr session, unsigned int msgID,
                          Json::Value &request) {
  Json::Value rsp;
  return rsp;
}
}; // namespace wim
