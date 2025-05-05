#include "Group.h"

#include "Const.h"
#include "DbGlobal.h"
#include "ImRpc.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"
#include <jsoncpp/json/value.h>
#include <spdlog/spdlog.h>
namespace wim {

Json::Value GroupCreate(ChatSession::Ptr session, unsigned int msgID,
                        Json::Value &request) {

  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_CREATE_RSP);
  });

  // gid should by server generate, todo...
  long gid = request["gid"].asInt64();
  long uid = request["uid"].asInt64();
  std::string groupName = request["name"].asString();
  int ret = db::MysqlDao::GetInstance()->hasGroup(gid);

  rsp["gid"] = request["gid"];
  if (ret == true) {
    rsp["error"] = ErrorCodes::GroupAlreadyExists;
    rsp["message"] = "群组已经存在";
    return rsp;
  } else if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
  }

  long sessionKey = db::RedisDao::GetInstance()->generateSessionId();
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
  }

  rsp["error"] = ErrorCodes::Success;
  rsp["sessionKey"] = Json::Value::Int64(sessionKey);
  return rsp;
}

Json::Value GroupNotifyJoin(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;

  long uid = request["uid"].asInt64();
  long gid = request["gid"].asInt64();
  std::string requestMessage = request["requestMessage"].asString();
  // 搜索群聊时存在，但加入群聊时未必存在
  int ret = db::MysqlDao::GetInstance()->hasGroup(gid);
  if (ret == false) {
    rsp["error"] = ErrorCodes::GroupNotExists;
    rsp["message"] = "群组不存在";
  }

  // 暂用好友相关表，消息将通知所有群管理
  std::string createTime = getCurrentDateTime();
  db::FriendApply::Ptr apply(new db::FriendApply(
      gid, uid, db::FriendApply::Status::Wait, requestMessage, createTime));

  ret = db::MysqlDao::GetInstance()->insertFriendApply(apply);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
  }

  db::GroupMember::MemberList managerList =
      db::MysqlDao::GetInstance()->getGroupRoleMemberList(
          gid, db::GroupMember::Role::Manager);
  for (auto manager : managerList) {
    Json::Value notifyInfo;
    bool isLocalMachineOnline =
        OnlineUser::GetInstance()->isOnline(manager->uid);
    if (isLocalMachineOnline) {
      auto user = OnlineUser::GetInstance()->GetUserSession(manager->uid);
      notifyInfo["fromUid"] = Json::Value::Int64(uid);
      notifyInfo["toId"] = Json::Value::Int64(manager->gid);
      notifyInfo["type"]; // todo
      notifyInfo["content"] = requestMessage;

      // 同好友申请一样，通知REQ发送到客户端则表示通知接收者
      long serverSeq = db::RedisDao::GetInstance()->generateMsgId();
      OnlineUser::GetInstance()->onReWrite(
          OnlineUser::ReWriteType::Message, serverSeq, manager->uid,
          notifyInfo.toStyledString(), ID_GROUP_NOTIFY_JOIN_REQ);
    }
    Json::Value userInfo =
        db::RedisDao::GetInstance()->getOnlineUserInfoObject(manager->uid);
    bool isOtherMachineOnline = !userInfo.empty();
    if (isOtherMachineOnline) {
      // rpc
      ///  ...
    }
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
}

Json::Value GroupPullNotify(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;
  rsp["gid"] = request["gid"];

  long gid = request["gid"].asInt64();

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
    long requestUid = apply->toUid;
    notifyInfo["uid"] = Json::Value::Int64(requestUid);
    //...
    rsp["applyList"].append(notifyInfo);
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
}

Json::Value GroupReplyJoin(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request) {
  Json::Value rsp;
  rsp["gid"] = request["gid"];

  long gid = request["gid"].asInt64();
  long managerUid = request["managerUid"].asInt64();
  long requestorUid = request["requestorUid"].asInt64();
  bool accept = request["accept"].asBool();

  // 搜索群聊时存在，但加入群聊时未必存在
  int ret = db::MysqlDao::GetInstance()->hasGroup(gid);
  if (ret == false) {
    rsp["error"] = ErrorCodes::GroupNotExists;
    rsp["message"] = "群组不存在";
  }

  // 暂用好友相关表，消息将通知所有群管理
  std::string createTime = getCurrentDateTime();
  db::FriendApply::Ptr apply(new db::FriendApply(
      gid, requestorUid, db::FriendApply::Status::Wait, "", createTime));

  ret = db::MysqlDao::GetInstance()->updateFriendApplyStatus(apply);
  if (ret == -1) {
    rsp["error"] = ErrorCodes::MysqlFailed;
    rsp["message"] = "数据库操作失败";
  }

  db::GroupMember::MemberList managerList =
      db::MysqlDao::GetInstance()->getGroupRoleMemberList(
          gid, db::GroupMember::Role::Manager);
  for (auto manager : managerList) {
    Json::Value notifyInfo;
    bool isLocalMachineOnline =
        OnlineUser::GetInstance()->isOnline(manager->uid);
    // 处理加入请求的管理者默认有通知，故忽略
    if (manager->uid == managerUid)
      continue;

    if (isLocalMachineOnline) {
      auto user = OnlineUser::GetInstance()->GetUserSession(manager->uid);
      notifyInfo["fromUid"] = Json::Value::Int64(requestorUid);
      notifyInfo["toId"] = Json::Value::Int64(manager->gid);
      notifyInfo["type"]; // todo
      notifyInfo["content"] = "";

      // 同好友申请一样，通知REQ发送到客户端则表示通知接收者
      long serverSeq = db::RedisDao::GetInstance()->generateMsgId();
      OnlineUser::GetInstance()->onReWrite(
          OnlineUser::ReWriteType::Message, serverSeq, manager->uid,
          notifyInfo.toStyledString(), ID_GROUP_NOTIFY_JOIN_REQ);
    }
    Json::Value userInfo =
        db::RedisDao::GetInstance()->getOnlineUserInfoObject(manager->uid);
    bool isOtherMachineOnline = !userInfo.empty();
    if (isOtherMachineOnline) {
      // rpc
      ///  ...
    }
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
}
Json::Value GroupQuit(ChatSession::Ptr session, unsigned int msgID,
                      Json::Value &request) {
  Json::Value rsp;
  return rsp;
}

Json::Value GroupTextSend(ChatSession::Ptr session, unsigned int msgID,
                          Json::Value &request) {
  Json::Value rsp;
  return rsp;
}

}; // namespace wim