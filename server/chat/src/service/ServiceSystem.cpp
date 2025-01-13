#include "ServiceSystem.h"
#include "ChatServer.h"
#include "Configer.h"
#include "Const.h"

#include "MysqlManager.h"
#include "OnlineUserManager.h"
#include "RedisManager.h"
#include "RpcClient.h"
#include <json/json.h>
#include <spdlog/spdlog.h>

namespace uitl {

bool IsIntergral(const std::string &str) {
  for (char c : str) {
    if (!std::isdigit(c)) {
      return false;
    }
  }
  return true;
}
}; // namespace uitl

ServiceSystem::ServiceSystem() : isStop(false) {
  Init();
  worker = std::thread(&ServiceSystem::run, this);
}

ServiceSystem::~ServiceSystem() {
  isStop = true;
  _consume.notify_one();
  worker.join();
}

void ServiceSystem::Init() {
  _serviceGroup[ID_CHAT_LOGIN_INIT] =
      std::bind(&ServiceSystem::LoginHandler, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_PING_PONG_REQ] =
      std::bind(&ServiceSystem::PingKeepAlive, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_SEARCH_USER_REQ] =
      std::bind(&ServiceSystem::SearchUser, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_ADD_FRIEND_REQ] =
      std::bind(&ServiceSystem::AddFriendApply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_AUTH_FRIEND_REQ] =
      std::bind(&ServiceSystem::AuthFriendApply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_PUSH_TEXT_MSG_REQ] =
      std::bind(&ServiceSystem::PushTextMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_ONLINE_PULL_REQ] =
      std::bind(&ServiceSystem::OnlinePullHandler, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);
}

void ServiceSystem::PingKeepAlive(std::shared_ptr<ChatSession> session,
                                  unsigned short msgID,
                                  const std::string &msgData) {}

void ServiceSystem::PushService(std::shared_ptr<protocol::LogicPackage> msg) {
  std::unique_lock<std::mutex> lock(_mutex);
  _messageGroup.push(msg);
  if (_messageGroup.size() == 1) {
    lock.unlock();
    _consume.notify_one();
  }
}

void ServiceSystem::run() {
  for (;;) {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_messageGroup.empty() && !isStop) {
      _consume.wait(lock);
    }

    if (isStop) {
      while (!_messageGroup.empty()) {
        auto package = _messageGroup.front();
        spdlog::info("[ServiceSystem::DoActive] recv service ID  is {}",
                     package->_recvPackage->id);

        auto call_back_iter = _serviceGroup.find(package->_recvPackage->id);
        if (call_back_iter == _serviceGroup.end()) {
          _messageGroup.pop();
          continue;
        }

        call_back_iter->second(package->_session, package->_recvPackage->id,
                               std::string(package->_recvPackage->data,
                                           package->_recvPackage->cur));
        _messageGroup.pop();
      }
      break;
    }

    auto package = _messageGroup.front();
    spdlog::info("[ServiceSystem::DoActive] recv_msg id  is {}",
                 package->_recvPackage->id);
    auto callbackIter = _serviceGroup.find(package->_recvPackage->id);
    if (callbackIter == _serviceGroup.end()) {
      _messageGroup.pop();
      spdlog::warn(
          "[ServiceSystem::DoActive] recv msg id {}, handler not found");
      continue;
    }
    callbackIter->second(
        package->_session, package->_recvPackage->id,
        std::string(package->_recvPackage->data, package->_recvPackage->cur));
    _messageGroup.pop();
  }
}

void ServiceSystem::LoginHandler(std::shared_ptr<ChatSession> session,
                                 unsigned short msgID,
                                 const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  parser.parse(msgData, req);

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_CHAT_LOGIN_INIT_RSP);
  });

  auto uid = req["uid"].asInt();
  auto token = req["token"].asString();

  spdlog::info(
      "[ServiceSystem::LoginHandle] user login uid is  {}, and token is ", uid,
      token);

  std::string tokenK = PREFIX_REDIS_USER_TOKEN + std::to_string(uid);
  std::string tokenV = "";

  bool hasToken = RedisManager::GetInstance()->Get(tokenK, tokenV);
  if (!hasToken) {
    spdlog::info(
        "[ServiceSystem::LoginHandle] this user no have find it token");
    rsp["error"] = ErrorCodes::UidInvalid;
    return;
  }

  if (tokenV != token) {
    spdlog::info("[ServiceSystem::LoginHandle] this user token is invalid");
    rsp["error"] = ErrorCodes::TokenInvalid;
    return;
  }

  // 暂不考虑用户基本信息缓存(redis)
  // std::string baseKey = PREFIX_REDIS_USER_INFO + uidStr;
  // auto userInfo = std::make_shared<UserInfo>();
  // ReidsManager::GetInstance()->Get(baseKey, userInfo);

  // 暂不考虑用户好友请求通知拉取(mysql)
  // std::vector<std::shared_ptr<ApplyInfo>> apply_list;
  // bool hasApplyList = GetFriendApplyInfo(uid, apply_list);

  // 暂不考虑好友列表拉取(mysql)
  // std::vector<std::shared_ptr<UserInfo>> friend_list = {};
  // bool hasFriendList = MysqlManager::GetInstance()->GetFriendList(uid,
  // friend_list);

  auto conf = Configer::getConfig("server");
  auto serverName = conf["selfServer"]["name"].as<std::string>();
  auto userActive = RedisManager::GetInstance()->HGet(
      PREFIX_REDIS_USER_ACTIVE_COUNT, serverName);

  auto count = userActive.empty() ? 0 : std::stoi(userActive);
  ++count;

  RedisManager::GetInstance()->HSet(PREFIX_REDIS_USER_ACTIVE_COUNT, serverName,
                                    std::to_string(count));
  session->SetUserId(uid);
  std::string ipKey = PREFIX_REDIS_UIP + std::to_string(uid);
  RedisManager::GetInstance()->Set(ipKey, serverName);
  OnlineUserManager::GetInstance()->MapUserSession(uid, session);

  return;
}

void ServiceSystem::SearchUser(std::shared_ptr<ChatSession> session,
                               unsigned short msgID, const string &msgData) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msgData, root);
  auto uid_str = root["uid"].asString();
  std::cout << "user SearchInfo uid is  " << uid_str << "\n";

  Json::Value rtvalue;

  Defer defer([&rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_SEARCH_USER_RSP);
  });

  // todo
}

void ServiceSystem::AddFriendApply(std::shared_ptr<ChatSession> session,
                                   unsigned short msgID,
                                   const string &msgData) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msgData, root);

  auto uid = root["uid"].asInt();
  auto applyname = root["applyname"].asString();
  auto bakname = root["bakname"].asString();
  auto touid = root["touid"].asInt();
  std::cout << "user login uid is  " << uid << " applyname  is " << applyname
            << " bakname is " << bakname << " touid is " << touid << "\n";

  Json::Value rtvalue;
  rtvalue["error"] = ErrorCodes::Success;
  Defer defer([&rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_ADD_FRIEND_RSP);
  });

  // MysqlManager::GetInstance()->AddFriendApply(uid, touid);

  auto to_str = std::to_string(touid);
  auto to_ip_key = PREFIX_REDIS_UIP + to_str;
  std::string to_ip_value = "";
  bool b_ip = RedisManager::GetInstance()->Get(to_ip_key, to_ip_value);
  if (!b_ip) {
    return;
  }

  auto conf = Configer::getConfig("Server");
  auto serverName = conf["SelfServer"]["Name"].as<std::string>();
  if (to_ip_value == serverName) {
    auto session = OnlineUserManager::GetInstance()->GetSession(touid);
    if (session) {
      Json::Value notify;
      notify["error"] = ErrorCodes::Success;
      notify["applyuid"] = uid;
      notify["name"] = applyname;
      notify["desc"] = "";
      std::string return_str = notify.toStyledString();
      session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
    }

    return;
  }

  std::string base_key = PREFIX_REDIS_USER_INFO + std::to_string(uid);
  auto apply_info = std::make_shared<UserInfo>();
  // bool b_info = GetBaseInfo(base_key, uid, apply_info);

  // AddFriendReq add_req;
  // add_req.set_applyuid(uid);
  // add_req.set_touid(touid);
  // add_req.set_name(applyname);
  // add_req.set_desc("");
  // if (b_info) {
  //   add_req.set_icon(apply_info->icon);
  //   add_req.set_sex(apply_info->sex);
  //   add_req.set_nick(apply_info->nick);
  // }

  // ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
}

void ServiceSystem::AuthFriendApply(std::shared_ptr<ChatSession> session,
                                    unsigned short msg_id,
                                    const string &msg_data) {
  // todo
}

void ServiceSystem::PushTextMessage(std::shared_ptr<ChatSession> session,
                                    unsigned short msgID,
                                    const string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  auto parseSuccess = parser.parse(msgData, req);
  if (!parseSuccess) {
    spdlog::error("[ServiceSystem::PushTextMessage] parse msg_data error!");
    return;
  }

  auto from = req["from"].asInt();
  auto to = req["to"].asInt();
  Json::Value message = req["message"];

  rsp["notify"] = "ACK";
  rsp["error"] = ErrorCodes::Success;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_TEXT_CHAT_MSG_RSP);
  });

  std::string tmp{};
  bool hasUser = RedisManager::GetInstance()->Get("online_user", tmp);
  auto toSession = OnlineUserManager::GetInstance()->GetSession(to);
  if (hasUser == true && toSession != nullptr) {
  }
  // auto toIpK = PREFIX_REDIS_UIP + std::to_string(to);
  // std::string toIpV = "";
  // bool isExist = RedisManager::GetInstance()->Get(toIpK, toIpV);
  // if (!isExist) {
  //   spdlog::info("[ServiceSystem::PushTextMessage] no find this user ip!");
  //   return;
  // }

  // auto conf = Configer::getConfig("server");

  // auto serverName = conf["selfServer"]["name"].as<std::string>();

  // if (toIpV == serverName) {
  //   spdlog::info("In local server, recv msg, notify to client");
  //   auto session = OnlineUserManager::GetInstance()->GetSession(to);
  //   if (session) {
  //     std::string return_str = rsp.toStyledString();
  //     session->Send(return_str, ID_NOTIFY_PUSH_TEXT_MSG_REQ);
  //   }
  // }

  // spdlog::info("[ServiceSystem::PushTextMessage] this user ip maybe in other
  // "
  //              "server, rpc-forward to other server...");

  // TextChatMsgReq rpcRequest;
  // rpcRequest.set_fromuid(from);
  // rpcRequest.set_touid(to);
  // for (const auto &node : message) {
  //   auto msgID = node["msgID"].asString();
  //   auto text = node["text"].asString();
  //   spdlog::info("[ServiceSystem::PushTextMessage] msgID is {}, text is {}",
  //                msgID, text);

  //   auto *text_msg = rpcRequest.add_textmsgs();
  //   text_msg->set_msgid(msgID);
  //   text_msg->set_msgcontent(text);
  // }

  // ChatGrpcClient::GetInstance()->NotifyTextChatMsg(toIpValue, rpcRequest,
  //                                                  package);
}

void ServiceSystem::OnlinePullHandler(std::shared_ptr<ChatSession> session,
                                      unsigned short msgID,
                                      const string &msgData) {

  Json::Reader reader;
  Json::Value root;
  reader.parse(msgData, root);

  auto uid = root["uid"].asInt();
  // todo: online pull logic
}