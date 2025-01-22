#include "Service.h"
#include "ChatServer.h"
#include "Configer.h"
#include "Const.h"

#include "OnlineUser.h"
#include "RedisManager.h"
#include "RpcClient.h"
#include "SqlOperator.h"
#include "json/value.h"
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <json/json.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <unordered_map>

ServiceSystem::ServiceSystem() : isStop(false) {
  Init();
  worker = std::thread(&ServiceSystem::Run, this);
}

ServiceSystem::~ServiceSystem() {
  isStop = true;
  _consume.notify_one();
  worker.join();
}

namespace util {
static std::atomic<size_t> seqGenerator(1);
static std::unordered_map<size_t, std::unique_ptr<net::steady_timer>>
    tansfTimerMap;
static std::unordered_map<size_t, size_t> reTansfCountMap;
static std::unordered_map<std::shared_ptr<ChatSession>,
                          std::shared_ptr<ChatSession>>
    sChannel;
} // namespace util

void OnRewriteTimer(std::shared_ptr<ChatSession> session, size_t seq,
                    Json::Value &rsp) {

  util::tansfTimerMap[seq] =
      std::make_unique<net::steady_timer>(session->GetIoc());
  util::reTansfCountMap[seq] = 0;
  auto &timer = util::tansfTimerMap[seq];
  timer->expires_from_now(std::chrono::seconds(5));
  timer->async_wait([session, rsp, seq](const error_code &ec) {
    if (ec == net::error::operation_aborted) {
      spdlog::error("[PushText] timer by cancelled");
      return;
    } else if (ec == net::error::timed_out) {
      if (util::reTansfCountMap[seq] >= 3) {
        spdlog::error("[PushText] retransf count exceed 3");
        return;
      }
      spdlog::info(
          "[OnRewriteTimer] timeout, on rewrite package, seq-id{}, count-{}",
          seq, util::reTansfCountMap[seq]);
      session->Send(rsp.toStyledString(), ID_TEXT_SEND_RSP);
      util::reTansfCountMap[seq]++;
    }
  });
}

void PushText(std::shared_ptr<ChatSession> toSession, size_t seq, int from,
              int to, const std::string &msg) {

  Json::Value rsp{};
  rsp["from"] = from;
  rsp["to"] = to;
  rsp["text"] = msg;
  toSession->Send(rsp.toStyledString(), ID_TEXT_SEND_RSP);

  OnRewriteTimer(toSession, seq, rsp);
}

bool SaveService(size_t seq, int from, int to, std::string msg) {
  // MysqlManager::GetInstance()->SaveService(from, to, msg);
  auto file = open("./service.log", O_CREAT | O_RDWR | O_APPEND);
  Json::Value rsp{};
  rsp["serviceID"] = ID_TEXT_CHAT_MSG_RSP;
  rsp["seq"] = static_cast<int>(seq);
  rsp["from"] = from;
  rsp["to"] = to;
  rsp["text"] = msg;

  char buf[4096]{};
  int rt = sprintf(buf, "From-%d|To-%d\t%s\n", from, to,
                   rsp.toStyledString().c_str());
  if (rt < 0) {
    spdlog::error("[ServiceSystem::SaveService] sprintf error");
    return false;
  }

  rt = write(file, buf, strlen(buf));

  if (rt < 0) {
    spdlog::error("[ServiceSystem::SaveService] write file error");
    return false;
  } else {
    spdlog::info("[ServiceSystem::SaveService] save service log success");
  }
  close(file);

  return true;
}

void PullText(size_t seq, int from, int to, const std::string &msg) {

  SaveService(util::seqGenerator, from, to, msg);
}

void AckHandle(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);

  Defer _([&rsp, session]() {
    auto rt = rsp.toStyledString();
    session->Send(rt, ID_UTIL_ACK_RSP);
  });

  if (!parserSuccess) {
    spdlog::error("[ServiceSystem::AckHandle] parse msg_data error!");
    rsp["error"] = ErrorCodes::Error_Json;
    return;
  }
  auto seq = req["seq"].asInt();
  if (util::tansfTimerMap.find(seq) == util::tansfTimerMap.end()) {
    spdlog::error(
        "[ServiceSystem::AckHandle] ack request is wrong for seq-id{}", seq);
    rsp["error"] = -1;
    return;
  }
  if (util::reTansfCountMap.find(seq) == util::reTansfCountMap.end()) {
    spdlog::error(
        "[ServiceSystem::AckHandle] retransf count not found for seq-id{}",
        seq);
    rsp["error"] = -1;
    return;
  }
  auto &timer = util::tansfTimerMap[seq];
  timer->cancel();
  util::tansfTimerMap.erase(seq);
  util::reTansfCountMap.erase(seq);
  rsp["error"] = ErrorCodes::Success;

  // rsp["seq"] = seq;
  // rsp["status"] = "Read";
  // rsp["error"] = ErrorCodes::Success;
  // util::sChannel[session]->Send(rsp.toStyledString(),
  // ID_TEXT_CHAT_MSG_RSP);

  // 若客户端发送了ack，服务端仍没收到，此时超时重发的消息会被客户端读取并分析处理
  // ，截止服务端收到ack，其中客户端包含ack重发次数逻辑，因此无需再发送
}

void TextSend(std::shared_ptr<ChatSession> session, unsigned int msgID,
              const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  Defer _([&rsp, session]() {
    auto rt = rsp.toStyledString();
    session->Send(rt, ID_TEXT_CHAT_MSG_RSP);
  });

  auto parseSuccess = parser.parse(msgData, req);
  if (!parseSuccess) {
    rsp["error"] = ErrorCodes::Error_Json;
    spdlog::error(
        "[ServiceSystem::TextSend] parse msg_data error! | msgData {}",
        msgData);
    return;
  }

  auto from = req["from"].asInt();
  auto to = req["to"].asInt();
  Json::Value message = req["text"];

  bool isOnline = OnlineUser::GetInstance()->isOnline(to);
  int seq = static_cast<int>(util::seqGenerator.load());
  if (isOnline) {
    rsp["seq"] = seq;
    rsp["status"] = "Unread-Push";
    rsp["error"] = ErrorCodes::Success;

    auto toSession = OnlineUser::GetInstance()->GetUser(to);
    util::sChannel[toSession] = session;

    PushText(toSession, util::seqGenerator, from, to, message.toStyledString());
  } else {
    rsp["seq"] = static_cast<int>(util::seqGenerator.load());
    rsp["status"] = "Unread-Pull";
    rsp["error"] = ErrorCodes::Success;

    PullText(util::seqGenerator, from, to, message.toStyledString());
  }

  OnRewriteTimer(session, seq, rsp);
  ++util::seqGenerator;
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

  _serviceGroup[ID_TEXT_SEND_REQ] =
      std::bind(&TextSend, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  _serviceGroup[ID_UTIL_ACK_SEQ] =
      std::bind(&AckHandle, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);
}

void ServiceSystem::PingKeepAlive(std::shared_ptr<ChatSession> session,
                                  unsigned int msgID,
                                  const std::string &msgData) {}

void ServiceSystem::PushService(std::shared_ptr<protocol::LogicPackage> msg) {
  std::unique_lock<std::mutex> lock(_mutex);
  _messageGroup.push(msg);
  if (_messageGroup.size() == 1) {
    lock.unlock();
    _consume.notify_one();
  }
}

void ServiceSystem::Run() {
  for (;;) {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_messageGroup.empty() && !isStop) {
      _consume.wait(lock);
    }

    if (isStop) {
      while (!_messageGroup.empty()) {
        auto package = _messageGroup.front();
        spdlog::info("[ServiceSystem::Run] recv service ID  is {}, service "
                     "Package is {}",
                     package->recvPackage->id, package->recvPackage->data);

        auto handleCall = _serviceGroup.find(package->recvPackage->id);
        if (handleCall == _serviceGroup.end()) {
          _messageGroup.pop();
          continue;
        }

        handleCall->second(
            package->session, package->recvPackage->id,
            std::string(package->recvPackage->data, package->recvPackage->cur));
        _messageGroup.pop();
      }
      break;
    }

    auto package = _messageGroup.front();
    spdlog::info("[ServiceSystem::Run] recv_msg id  is {}",
                 package->recvPackage->id);
    auto callbackIter = _serviceGroup.find(package->recvPackage->id);
    if (callbackIter == _serviceGroup.end()) {
      _messageGroup.pop();
      spdlog::warn("[ServiceSystem::Run] recv msg id {}, handler not found");
      continue;
    }
    callbackIter->second(
        package->session, package->recvPackage->id,
        std::string(package->recvPackage->data, package->recvPackage->cur));
    _messageGroup.pop();
  }
}

void ServiceSystem::LoginHandler(std::shared_ptr<ChatSession> session,
                                 unsigned int msgID,
                                 const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::Error_Json;
    spdlog::error("[ServiceSystem::LoginHandle] parse msg_data error!");
    return;
  }

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_CHAT_LOGIN_INIT_RSP);
  });

  auto uid = req["uid"].asInt();
  // auto token = req["token"].asString();

  rsp["error"] = ErrorCodes::Success;
  // 应用层上的用户管理，而在ChatServer中则是对传输层连接的管理
  OnlineUser::GetInstance()->MapUser(uid, session);
  spdlog::info(
      "[ServiceSystem::LoginHandle] user logining, <uid: {} => session >", uid);

  // spdlog::info(
  //     "[ServiceSystem::LoginHandle] user login uid is  {}, and token is
  //     ", uid, token);

  // std::string tokenK = PREFIX_REDIS_USER_TOKEN + std::to_string(uid);
  // std::string tokenV = "";

  // bool hasToken = RedisManager::GetInstance()->Get(tokenK, tokenV);
  // if (!hasToken) {
  //   spdlog::info(
  //       "[ServiceSystem::LoginHandle] this user no have find it token");
  //   rsp["error"] = ErrorCodes::UidInvalid;
  //   return;
  // }

  // if (tokenV != token) {
  //   spdlog::info("[ServiceSystem::LoginHandle] this user token is
  //   invalid"); rsp["error"] = ErrorCodes::TokenInvalid; return;
  // }

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

  // auto conf = Configer::getConfig("server");
  // auto serverName = conf["selfServer"]["name"].as<std::string>();
  // auto userActive = RedisManager::GetInstance()->HGet(
  //     PREFIX_REDIS_USER_ACTIVE_COUNT, serverName);

  // auto count = userActive.empty() ? 0 : std::stoi(userActive);
  // ++count;

  // RedisManager::GetInstance()->HSet(PREFIX_REDIS_USER_ACTIVE_COUNT,
  // serverName,
  //                                   std::to_string(count));
  // session->SetUserId(uid);
  // std::string ipKey = PREFIX_REDIS_UIP + std::to_string(uid);
  // RedisManager::GetInstance()->Set(ipKey, serverName);
  // OnlineUser::GetInstance()->MapUserSession(uid, session);

  return;
}

void ServiceSystem::SearchUser(std::shared_ptr<ChatSession> session,
                               unsigned int msgID, const std::string &msgData) {
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
                                   unsigned int msgID,
                                   const std::string &msgData) {
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
    auto session = OnlineUser::GetInstance()->GetUser(touid);
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
                                    unsigned int msg_id,
                                    const std::string &msg_data) {
  // todo
}
void ServiceSystem::PushTextMessage(std::shared_ptr<ChatSession> session,
                                    unsigned int msgID,
                                    const std::string &msgData) {
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
  auto toSession = OnlineUser::GetInstance()->GetUser(to);
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

  // spdlog::info("[ServiceSystem::PushTextMessage] this user ip maybe in
  // other
  // "
  //              "server, rpc-forward to other server...");

  // TextChatMsgReq rpcRequest;
  // rpcRequest.set_fromuid(from);
  // rpcRequest.set_touid(to);
  // for (const auto &node : message) {
  //   auto msgID = node["msgID"].asString();
  //   auto text = node["text"].asString();
  //   spdlog::info("[ServiceSystem::PushTextMessage] msgID is {}, text is
  //   {}",
  //                msgID, text);

  //   auto *text_msg = rpcRequest.add_textmsgs();
  //   text_msg->set_msgid(msgID);
  //   text_msg->set_msgcontent(text);
  // }

  // ChatGrpcClient::GetInstance()->NotifyTextChatMsg(toIpValue, rpcRequest,
  //                                                  package);
}

void ServiceSystem::OnlinePullHandler(std::shared_ptr<ChatSession> session,
                                      unsigned int msgID,
                                      const std::string &msgData) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msgData, root);

  auto uid = root["uid"].asInt();
  // todo: online pull logic
}