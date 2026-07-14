#include "UserService.h"

#include "Const.h"
#include "DbGlobal.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"

namespace wim {

TcpPacket UserService::Search(ChatSession::Ptr session, uint32_t msgID,
                              TcpPacket &request) {
  TcpPacket rsp;
  auto username = request.username();
  auto user = db::MysqlDao::GetInstance()->getUser(username);
  if (user == nullptr) {
    rsp.set_error(-1);
    return rsp;
  }
  auto userInfo = db::MysqlDao::GetInstance()->getUserInfo(user->uid);
  if (userInfo == nullptr) {
    rsp.set_error(-1);
    return rsp;
  }
  rsp.set_uid(userInfo->uid);
  rsp.set_username(user->username);
  rsp.set_age(userInfo->age);
  rsp.set_head_image_url(userInfo->headImageURL);
  rsp.set_error(0);
  return rsp;
}

TcpPacket UserService::Quit(ChatSession::Ptr session, uint32_t msgID,
                            TcpPacket &request) {
  auto uid = request.uid();

  /*
  清理在线资源，网络层资源在对方close关闭时自行清理
  每个用户都有心跳机制，此处默认清理
  */
  OnlineUser::GetInstance()->ClearUser(uid, uid, session);
  return {};
}

TcpPacket UserService::ReLogin(int64_t uid, ChatSession::Ptr oldSession,
                               ChatSession::Ptr newSession) {
  TcpPacket rsp;
  return rsp;
}

TcpPacket UserService::Login(ChatSession::Ptr session, uint32_t msgID,
                             TcpPacket &request) {
  TcpPacket rsp;
  int64_t uid = request.uid();
  bool isFirstLogin = request.init();
  int status = 0;

  if (!session || uid <= 0) {
    rsp.set_error(ErrorCodes::UidInvalid);
    return rsp;
  }

  // token 校验和一次性绑定必须早于在线路由等用户态副作用。
  if (session->IsAuthenticated()) {
    if (session->GetUserId() != uid) {
      rsp.set_error(ErrorCodes::UidInvalid);
      rsp.set_message("session is already bound to another user");
      return rsp;
    }
  } else if (!request.has_auth_token() ||
             !db::RedisDao::GetInstance()->validateChatAuthToken(
                 uid, request.auth_token())) {
    rsp.set_error(ErrorCodes::TokenInvalid);
    rsp.set_message("invalid or expired chat auth token");
    return rsp;
  }
  if (!session->BindUserId(uid)) {
    rsp.set_error(ErrorCodes::UidInvalid);
    rsp.set_message("session is already bound to another user");
    return rsp;
  }

  // 待实现，先不做处理
  status = OnlineUser::GetInstance()->isOnline(uid);
  if (false && status == false) {
    rsp.set_error(ErrorCodes::UserOnline);
    auto oldSession = OnlineUser::GetInstance()->GetUserSession(uid);
    ReLogin(uid, oldSession, session);
  }

  status = db::RedisDao::GetInstance()->getOnlineUserInfo(uid).empty();
  // 分布式情况，待实现
  if (false && status) {
    rsp.set_error(ErrorCodes::Success);
  }

  // 用户信息处理
  db::UserInfo::Ptr userInfo;
  if (isFirstLogin) {
    // 首次登录，需要同步用户信息
    std::string name = request.name();
    short age = request.age();
    std::string sex = request.sex();
    userInfo.reset(new db::UserInfo(uid, name, age, sex, {}));

    status = db::MysqlDao::GetInstance()->insertUserInfo(userInfo);
    if (status != 0) {
      LOG_WARN(wim::businessLogger, "插入用户信息失败, uid-{} ", uid);
      rsp.set_error(-1);
      return rsp;
    }
  } else {
    userInfo = db::MysqlDao::GetInstance()->getUserInfo(uid);
    if (userInfo == nullptr) {
      LOG_WARN(wim::businessLogger, "获取用户信息失败, uid-{} ", uid);
      rsp.set_error(-1);
      return rsp;
    }
  }

  // 建立<userInfo, session>用户网络线路映射
  status = OnlineUser::GetInstance()->MapUser(userInfo, session);
  if (status == false) {
    rsp.set_error(-1);
    return rsp;
  }
  rsp.set_uid(userInfo->uid);
  rsp.set_name(userInfo->name);
  rsp.set_age(userInfo->age);
  rsp.set_sex(userInfo->sex);
  rsp.set_head_image_url(userInfo->headImageURL);
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

}  // namespace wim
