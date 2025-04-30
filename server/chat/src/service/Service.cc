#include "Service.h"
#include "ChatSession.h"
#include "Const.h"
#include "DbGlobal.h"
#include "ImRpc.h"
#include "KafkaOperator.h"
#include "Logger.h"
#include "OnlineUser.h"
#include "Redis.h"

#include "Friend.h"

#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

namespace wim {
Service::Service() : isStop(false) {
  Init();
  worker = std::thread(&Service::Run, this);
}

Service::~Service() {
  isStop = true;
  consume.notify_one();
  worker.join();
}

/*
接口说明:
  1、函数处理分为两类：直接处理或转发处理
  2、直接处理时，单向发送请求者响应包
  2、转发处理时，先单向的发送请求响应包，再通过rpc换发间接响应目标接收者
  3、带有转发性质的函数会被复用，复用时请求方会话默认为空，因其已被响应，例如：
      wim::ReplyAddFriend(nullptr, ID_REPLY_ADD_FRIEND_REQ, requestJsonData);
  4、这意味着带有转发性质的函数，不得在函数内部引用session，因其在转发时默认为空
*/
void Service::Init() {
  // 已成功
  serviceGroup[ID_LOGIN_INIT_REQ] =
      std::bind(&OnLogin, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_NOTIFY_ADD_FRIEND_REQ] =
      std::bind(wim::NotifyAddFriend, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  serviceGroup[ID_PING_REQ] =
      std::bind(&PingHandle, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  // 待测试

  serviceGroup[ID_REPLY_ADD_FRIEND_REQ] =
      std::bind(wim::ReplyAddFriend, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  serviceGroup[ID_PULL_FRIEND_LIST_REQ] =
      std::bind(&pullFriendList, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_PULL_FRIEND_APPLY_LIST_REQ] =
      std::bind(&pullFriendApplyList, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  serviceGroup[ID_PULL_MESSAGE_LIST_REQ] =
      std::bind(&pullMessageList, std::placeholders::_1, std::placeholders::_2,

                std::placeholders::_3);
  serviceGroup[ID_TEXT_SEND_REQ] =
      std::bind(&TextSend, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_ACK] =
      std::bind(&AckHandle, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);
}

void Service::PushService(std::shared_ptr<NetworkMessage> msg) {
  std::unique_lock<std::mutex> lock(_mutex);
  messageQueue.push(msg);
  KafkaProducer::GetInstance()->SaveMessage(msg->getData());
  if (messageQueue.size() == 1) {
    lock.unlock();
    consume.notify_one();
  }
}

void Service::Run() {
  for (;;) {
    std::unique_lock<std::mutex> lock(_mutex);
    while (messageQueue.empty() && !isStop) {
      consume.wait(lock);
    }

    if (isStop) {
      while (!messageQueue.empty()) {
        auto package = messageQueue.front();
        unsigned int id = package->protocolData->id;
        const char *data = package->protocolData->data;
        unsigned int dataSize = package->protocolData->getDataSize();
        LOG_INFO(wim::businessLogger,
                 "read service ID  is {}, service "
                 "Package is {}",
                 id, data);

        auto handleCaller = serviceGroup.find(id);
        if (handleCaller == serviceGroup.end()) {
          LOG_INFO(wim::businessLogger, "not found! service ID  is {}, ", id);
          messageQueue.pop();
          continue;
        }

        std::string msgData{data, dataSize};
        Json::Reader reader;
        Json::Value request, response;

        bool parserSuccess = reader.parse(msgData, request);
        if (!parserSuccess) {
          LOG_WARN(wim::businessLogger, "parse message data error!, id: {}",
                   id);
          Json::Value rsp;
          rsp["error"] = ErrorCodes::JsonParser;
          package->contextSession->Send(rsp.toStyledString(), id);
          messageQueue.pop();
          continue;
        }

        LOG_DEBUG(wim::businessLogger, "msgId: {}, request: {}", id,
                  request.toStyledString());

        response = handleCaller->second(package->contextSession, id, request);
        auto ret = response.toStyledString();
        package->contextSession->Send(ret,
                                      __getServiceResponseId(ServiceID(id)));

        LOG_DEBUG(wim::businessLogger, "msgId: {}, response: {}",
                  response.toStyledString());

        messageQueue.pop();
      }
      break;
    }

    auto package = messageQueue.front();
    unsigned int id = package->protocolData->id;
    const char *data = package->protocolData->data;
    unsigned int dataSize = package->protocolData->getDataSize();
    LOG_INFO(wim::businessLogger,
             "read service ID  is {}, service "
             "Package is {}",
             id, data);

    auto handleCall = serviceGroup.find(id);
    if (handleCall == serviceGroup.end()) {
      LOG_INFO(wim::businessLogger, "not found! service ID  is {}, ", id);

      Json::Value rsp;
      rsp["error"] = ErrorCodes::NotFound;
      package->contextSession->Send(rsp.toStyledString(), id);
      messageQueue.pop();
      continue;
    }

    std::string msgData{data, dataSize};
    Json::Reader reader;
    Json::Value request;

    bool parserSuccess = reader.parse(msgData, request);
    if (!parserSuccess) {
      LOG_WARN(wim::businessLogger, "parse message data error!, id: {}", id);

      Json::Value rsp;
      rsp["error"] = ErrorCodes::JsonParser;
      package->contextSession->Send(rsp.toStyledString(), id);
      messageQueue.pop();
      continue;
    }

    handleCall->second(package->contextSession, id, request);
    messageQueue.pop();
  }
}

Json::Value PingHandle(ChatSession::Ptr session, unsigned int msgID,
                       Json::Value &request) {
  Json::Value rsp;
  long uid = request["uid"].asInt64();

  OnlineUser::GetInstance()->Pong(uid);
  return rsp;
}

int PushText(ChatSession::Ptr toSession, size_t seq, int from, int to,
             const std::string &msg, int msgID) {

  toSession->Send(msg, msgID);
  return 0;
}

bool SaveService(size_t seq, int from, int to, std::string msg) {
  auto file = open("./service.log", O_CREAT | O_RDWR | O_APPEND);
  char buf[4096]{};
  int rt = sprintf(buf, "From-%d|To-%d\t%s\n", from, to, msg.c_str());
  if (rt < 0) {
    spdlog::error("[SaveService] sprintf error");
    return false;
  }

  rt = write(file, buf, strlen(buf));

  if (rt < 0) {
    spdlog::error("[SaveService] write file error");
    return false;
  } else {
    LOG_INFO(wim::businessLogger, "[SaveService] save service log success");
  }
  close(file);

  return true;
}

// 待验证
Json::Value AckHandle(ChatSession::Ptr session, unsigned int msgID,
                      Json::Value &request) {
  Json::Value rsp;
  long seq = request["seq"].asInt64();

  OnlineUser::GetInstance()->cancelAckTimer(seq);
}

Json::Value SerachUser(ChatSession::Ptr session, unsigned int msgID,
                       Json::Value &request) {
  Json::Value rsp;
  auto username = request["username"].asString();
  auto user = db::MysqlDao::GetInstance()->getUser(username);
  if (user == nullptr) {
    rsp["error"] = -1;
    return rsp;
  }
  auto userInfo = db::MysqlDao::GetInstance()->getUserInfo(user->uid);
  if (userInfo == nullptr) {
    rsp["error"] = -1;
    return rsp;
  }
  rsp["uid"] = Json::Value::Int64(userInfo->uid);
  rsp["username"] = user->username;
  rsp["age"] = userInfo->age;
  rsp["headImageURL"] = userInfo->headImageURL;
  rsp["error"] = 0;

  return rsp;
}

// 待验证
Json::Value TextSend(ChatSession::Ptr session, unsigned int msgID,
                     Json::Value &request) {

  Json::Value rsp;

  long clientSeq = request["seq"].asInt64();
  long from = request["fromUid"].asInt64();
  long to = request["toUid"].asInt64();
  long sessionHashKey = request["sessionKey"].asInt64();
  std::string text = request["text"].asString();
  LOG_INFO(businessLogger,
           "from: {}, to: {}, clientSeq: {}, sessionKey: {}, text: {}", from,
           to, clientSeq, sessionHashKey, text);

  bool hasUserMsgId =
      db::RedisDao::GetInstance()->getUserMsgId(from, clientSeq);
  static short __expireUserMsgId = 10;
  if (hasUserMsgId) {
    LOG_INFO(businessLogger, "重复消息, 客户端消息序列号为: {}", clientSeq);
    db::RedisDao::GetInstance()->expireUserMsgId(from, clientSeq,
                                                 __expireUserMsgId);
    rsp["error"] = ErrorCodes::RepeatMessage;
    rsp["message"] = "重复消息";
    return rsp;
  }

  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);

  long serverMsgSeq = 0;
  LOG_INFO(businessLogger, "用户在线，转发消息, 用户ID: {}", to);
  if (isLocalMachineOnline) {
    db::RedisDao::GetInstance()->setUserMsgId(from, clientSeq,
                                              __expireUserMsgId);
    serverMsgSeq = db::RedisDao::GetInstance()->generateMsgId();
    rsp["status"] = "wait";
    rsp["error"] = ErrorCodes::Success;

    // 替换成服务端消息序列号，因需维护服务端道接收者的消息查收状态
    request["seq"] = Json::Value::Int64(serverMsgSeq);
    OnlineUser::GetInstance()->onReWrite(
        OnlineUser::ReWriteType::Message, serverMsgSeq, to,
        request.toStyledString(), ID_TEXT_SEND_REQ);
    LOG_INFO(businessLogger, "响应发送者，消息：{}", rsp.toStyledString());
    return rsp;
  } else {
    LOG_INFO(businessLogger, "用户不在线，存储离线消息, 用户ID: {}", to);
    db::Message::Ptr message(new db::Message(
        serverMsgSeq, from, to, std::to_string(sessionHashKey),
        db::Message::Type::TEXT, text, db::Message::Status::WAIT));
    db::MysqlDao::GetInstance()->insertMessage(message);
    rsp["seq"] = Json::Value::Int64(clientSeq);
    rsp["status"] = "wait";
    rsp["error"] = ErrorCodes::Success;
  }

  std::string userInfo = db::RedisDao::GetInstance()->getOnlineUserInfo(to);
  bool isOtherMachineOnline = userInfo.empty();
  if (isOtherMachineOnline) {
    LOG_INFO(businessLogger, "用户不在本地机器，转发消息到其他机器, 用户ID: {}",
             to);
    // imrpc...
    rpc::TextSendMessageRequest rpcRequest;
    rpc::TextSendMessageResponse rpcResponse;
    rpcRequest.set_fromuid(from);
    rpcRequest.set_touid(to);
    rpcRequest.set_text(text);

    rpcResponse = rpc::ImRpc::GetInstance()
                      ->getRpc(sessionHashKey)
                      ->forwardTextSendMessage(rpcRequest);

    std::string status = rpcResponse.status();
    LOG_INFO(businessLogger, "rpc response: {}, status: {}",
             rpcResponse.DebugString(), status);
  }

  return rsp;
}

Json::Value UserQuit(ChatSession::Ptr session, unsigned int msgID,
                     Json::Value &request) {
  Json::Value rsp;

  auto uid = request["uid"].asInt64();

  auto userSession = OnlineUser::GetInstance()->GetUserSession(uid);

  /*
  清理在线资源，网络层资源在对方close关闭时自行清理
  每个用户都有心跳机制，此处默认清理
  */
  OnlineUser::GetInstance()->ClearUser(uid, uid);
  return rsp;
}

Json::Value ReLogin(long uid, ChatSession::Ptr oldSession,
                    ChatSession::Ptr newSession) {
  Json::Value rsp;

  return rsp;
}

Json::Value OnLogin(ChatSession::Ptr session, unsigned int msgID,
                    Json::Value &request) {
  Json::Value rsp;
  long uid = request["uid"].asInt64();
  int status = 0;

  // 待实现，先不做处理
  status = OnlineUser::GetInstance()->isOnline(uid);
  if (false && status == false) {

    rsp["error"] = ErrorCodes::UserOnline;
    auto oldSession = OnlineUser::GetInstance()->GetUserSession(uid);
    ReLogin(uid, oldSession, session);

    LOG_INFO(wim::businessLogger, "[LoginHandle] THIS USER IS ONLINE, uid-{}",
             uid);
  }

  status = db::RedisDao::GetInstance()->getOnlineUserInfo(uid).empty();
  // 分布式情况，待实现
  if (false && status) {
    rsp["error"] = ErrorCodes::Success;
  }

  // 用户信息处理
  bool isFirstLogin = request["init"].asBool();
  db::UserInfo::Ptr userInfo;
  if (isFirstLogin) {
    // 首次登录，需要同步用户信息
    std::string name = request["name"].asString();
    short age = request["age"].asInt();
    std::string sex = request["sex"].asString();
    std::string headImageURL = request["headImageURL"].asString();
    userInfo.reset(new db::UserInfo(uid, name, age, sex, headImageURL));

    status = db::MysqlDao::GetInstance()->insertUserInfo(userInfo);
    if (status != 0) {
      LOG_WARN(wim::businessLogger, "insert user info failed, uid-{} ", uid);
      rsp["error"] = -1;
      return rsp;
    }
  } else {
    userInfo = db::MysqlDao::GetInstance()->getUserInfo(uid);
    if (userInfo == nullptr) {
      LOG_WARN(wim::businessLogger, "get user info failed, uid-{} ", uid);
      rsp["error"] = -1;
      return rsp;
    }
  }

  // 建立<userInfo, session>用户网络线路映射
  status = OnlineUser::GetInstance()->MapUser(userInfo, session);
  if (status == false) {
    rsp["error"] = -1;
    return rsp;
  }

  rsp["uid"] = Json::Value::Int64(userInfo->uid);
  rsp["name"] = userInfo->name;
  rsp["age"] = userInfo->age;
  rsp["sex"] = userInfo->sex;
  rsp["headImageURL"] = userInfo->headImageURL;
  rsp["error"] = ErrorCodes::Success;
  return rsp;
}

Json::Value pullFriendApplyList(ChatSession::Ptr session, unsigned int msgID,
                                Json::Value &request) {
  Json::Value rsp;

  long uid = request["uid"].asInt64();
  db::FriendApply::FriendApplyGroup applyList =
      db::MysqlDao::GetInstance()->getFriendApplyList(uid);
  if (applyList->empty()) {
    LOG_INFO(businessLogger, "apply list is empty, uid: {}", uid);
  }

  for (auto applyObject : *applyList) {
    Json::Value root;
    root["fromUid"] = Json::Value::Int64(applyObject->fromUid);
    root["toUid"] = Json::Value::Int64(applyObject->toUid);
    root["status"] = applyObject->status;
    root["content"] = applyObject->content;
    root["applyDateTime"] = applyObject->createTime;
    rsp["applyList"].append(root);
  }
  rsp["error"] = ErrorCodes::Success;

  return rsp;
}

Json::Value pullFriendList(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request) {
  Json::Value rsp;

  long uid = request["uid"].asInt64();
  db::Friend::FriendGroup friendList =
      db::MysqlDao::GetInstance()->getFriendList(uid);

  if (friendList->empty()) {
    LOG_INFO(wim::businessLogger, "friend list is empty, uid-{}", uid);
  }

  for (auto friendObject : *friendList) {
    long friendUid = friendObject->uidB;
    db::UserInfo::Ptr friendInfo =
        db::MysqlDao::GetInstance()->getUserInfo(friendUid);
    if (friendInfo != nullptr) {
      Json::Value root;
      root["uid"] = Json::Value::Int64(friendUid);
      root["name"] = friendInfo->name;
      root["age"] = friendInfo->age;
      root["sex"] = friendInfo->sex;
      root["headImageURL"] = friendInfo->headImageURL;
      rsp["friendList"].append(root);
    }
  }

  rsp["error"] = ErrorCodes::Success;
  return rsp;
}
Json::Value pullMessageList(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request) {
  Json::Value rsp;

  long from = request["fromUid"].asInt64();
  long to = request["toId"].asInt64();
  long lastMsgId = request["lasgMsgId"].asInt64();
  int limit = request["limit"].asInt();

  auto messageList =
      db::MysqlDao::GetInstance()->getUserMessage(from, to, lastMsgId, limit);
  if (messageList->empty()) {
    LOG_INFO(
        wim::businessLogger,
        "message list is empty, from: {}, to: {}, lastMsgId: {}, limit: {}",
        from, to, lastMsgId, limit);
  }

  rsp["toId"] = Json::Value::Int64(to);
  for (auto message : *messageList) {
    Json::Value tmp;
    tmp["messageId"] = Json::Value::Int64(message->messageId);
    tmp["type"] = message->type;
    tmp["content"] = message->content;
    tmp["status"] = message->status;
    tmp["sendDateTime"] = message->sendDateTime;
    tmp["readDateTime"] = message->readDateTime;

    rsp["messageList"].append(rsp);
  }
  rsp["error"] = ErrorCodes::Success;

  return rsp;
}

}; // namespace wim
