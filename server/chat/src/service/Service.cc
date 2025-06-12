#include "Service.h"
#include "ChatSession.h"
#include "Const.h"
#include "DbGlobal.h"
#include "FileRpc.h"
#include "Friend.h"
#include "Group.h"
#include "ImRpc.h"
#include "Logger.h"
#include "Mysql.h"
#include "OnlineUser.h"
#include "Redis.h"

#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstdio>
#include <fcntl.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <memory>
#include <string>

namespace wim {

Service::Service() : stopEnable(false) {
  Init();

  worker = std::thread(&Service::Run, this);
}

Service::~Service() {
  stopEnable = true;
  consume.notify_one();
  worker.join();
}

void Service::RegisterHandle(uint32_t msgID, HandleType handle) {
  if (serviceGroup.find(msgID) != serviceGroup.end()) {
    LOG_WARN(businessLogger, "该服务已注册,msgID: {}", msgID);
    return;
  }
  serviceGroup[msgID] = std::bind(handle, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
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
  /* 已成功 */

  // 状态
  RegisterHandle(ID_LOGIN_INIT_REQ, OnLogin);
  RegisterHandle(ID_USER_QUIT_REQ, UserQuit);
  RegisterHandle(ID_PING_REQ, PingHandle);
  RegisterHandle(ID_ACK, AckHandle);

  // 传输
  RegisterHandle(ID_TEXT_SEND_REQ, TextSend);
  RegisterHandle(ID_FILE_UPLOAD_REQ, UploadFile);

  // 好友
  RegisterHandle(ID_NOTIFY_ADD_FRIEND_REQ, NotifyAddFriend);
  RegisterHandle(ID_REPLY_ADD_FRIEND_REQ, ReplyAddFriend);

  // 拉取
  RegisterHandle(ID_PULL_FRIEND_LIST_REQ, pullFriendList);
  RegisterHandle(ID_PULL_FRIEND_APPLY_LIST_REQ, pullFriendApplyList);
  RegisterHandle(ID_PULL_SESSION_MESSAGE_LIST_REQ, pullSessionMessageList);
  RegisterHandle(ID_PULL_MESSAGE_LIST_REQ, pullMessageList);

  // 群聊
  RegisterHandle(ID_GROUP_CREATE_REQ, GroupCreate);
  RegisterHandle(ID_GROUP_NOTIFY_JOIN_REQ, GroupNotifyJoin);
  RegisterHandle(ID_GROUP_REPLY_JOIN_REQ, GroupReplyJoin);

  /* 待实现 */
  RegisterHandle(ID_GROUP_TEXT_SEND_REQ, GroupTextSend);
  RegisterHandle(ID_FILE_SEND_REQ, FileSend);
}

void Service::PushService(std::shared_ptr<Channel> msg) {
  std::unique_lock<std::mutex> lock(_mutex);
  messageQueue.push(msg);
  if (messageQueue.size() == 1) {
    lock.unlock();
    consume.notify_one();
  }
}

void Service::Run() {
  auto lam = [&]() {
    auto channel = messageQueue.front();
    uint32_t id = channel->protocolData->id;
    const char *data = channel->protocolData->data;
    uint32_t dataSize = channel->protocolData->getDataSize();

    auto handleCaller = serviceGroup.find(id);
    if (handleCaller == serviceGroup.end()) {
      LOG_INFO(wim::businessLogger, "没有这样的服务，ID： {}, ", id);

      Json::Value rsp;
      rsp["error"] = ErrorCodes::NotFound;
      channel->contextSession->Send(rsp.toStyledString(), id);
      messageQueue.pop();
      return;
    }

    std::string msgData{data, dataSize};
    Json::Reader reader;
    Json::Value request, response;
    int responseId = __getServiceResponseId(ServiceID(id));
    std::string requestIdMessage = getServiceIdString(id),
                responseIdMessage = getServiceIdString(responseId);

    bool parserSuccess = reader.parse(msgData, request);
    if (!parserSuccess) {
      LOG_WARN(wim::businessLogger, "消息解析失败, 请求服务: {}，消息：{}",
               requestIdMessage, msgData);
      Json::Value rsp;
      rsp["error"] = ErrorCodes::JsonParser;
      channel->contextSession->Send(rsp.toStyledString(), responseId);
      messageQueue.pop();
      return;
    }
    LOG_DEBUG(wim::businessLogger, "解析成功，请求服务: {}, 请求数据: {}",
              requestIdMessage, request.toStyledString());

    response = handleCaller->second(channel->contextSession, id, request);
    auto ret = response.toStyledString();
    channel->contextSession->Send(ret, responseId);

    LOG_DEBUG(wim::businessLogger, "响应服务: {}, 响应数据: {}",
              responseIdMessage, response.toStyledString());

    messageQueue.pop();
  };

  for (;;) {
    std::unique_lock<std::mutex> lock(_mutex);
    while (messageQueue.empty() && !stopEnable) {
      consume.wait(lock);
    }

    if (stopEnable) {
      while (!messageQueue.empty()) {
        lam();
      }
      break;
    }
    lam();
  }
}

Json::Value PingHandle(ChatSession::Ptr session, uint32_t msgID,
                       Json::Value &request) {
  Json::Value rsp;
  int64_t uid = request["uid"].asInt64();

  OnlineUser::GetInstance()->Pong(uid);
  return rsp;
}

Json::Value AckHandle(ChatSession::Ptr session, uint32_t msgID,
                      Json::Value &request) {
  Json::Value rsp;
  int64_t seq = request["seq"].asInt64();
  int64_t uid = request["uid"].asInt64();

  OnlineUser::GetInstance()->cancelAckTimer(seq, uid);
  rsp["error"] = ErrorCodes::Success;
  return rsp;
}

Json::Value SerachUser(ChatSession::Ptr session, uint32_t msgID,
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

Json::Value UploadFile(ChatSession::Ptr session, uint32_t msgID,
                       Json::Value &request) {
  Json::Value rsp;
  int64_t clientSeq = request["seq"].asInt64();
  int64_t uid = request["uid"].asInt64();
  std::string data = request["data"].asString();
  std::string fileName = request["fileName"].asString();
  rpc::FileType type;

  rsp["seq"] = Json::Value::Int64(clientSeq);

  bool hasUserMsgId = db::RedisDao::GetInstance()->getUserMsgId(uid, clientSeq);
  static short __expireUserMsgId = 10;
  if (hasUserMsgId) {
    LOG_INFO(businessLogger, "重复消息, 客户端消息序列号为: {}", clientSeq);
    db::RedisDao::GetInstance()->expireUserMsgId(uid, clientSeq,
                                                 __expireUserMsgId);
    rsp["error"] = ErrorCodes::RepeatMessage;
    rsp["message"] = "重复消息";
    return rsp;
  }

  std::string tmpType = request["type"].asString();
  if (tmpType == "TEXT") {
    type = rpc::FileType::TEXT;
  } else if (tmpType == "IMAGE") {
    type = rpc::FileType::IMAGE;
  } else {
    LOG_ERROR(businessLogger, "文件传输错误");
    rsp["error"] = ErrorCodes::FileTypeError;
    rsp["message"] = "文件类型错误";
    return rsp;
  }

  rpc::UploadRequest rpcRequest;
  rpc::UploadResponse rpcResponse;

  // 当set_allocated_chunk时，protobuf会自动管理 chunk 内存
  rpc::FileChunk *fileChunk = new rpc::FileChunk();
  fileChunk->set_seq(clientSeq);
  fileChunk->set_filename(fileName);
  fileChunk->set_type(type);
  fileChunk->set_data(data);

  rpcRequest.set_user_id(uid);
  rpcRequest.set_allocated_chunk(fileChunk);

  try {
    grpc::Status status =
        rpc::FileRpc::GetInstance()->forwardUpload(rpcRequest, rpcResponse);

    if (status.ok()) {
      rsp["error"] = ErrorCodes::Success;
    } else {
      rsp["error"] = ErrorCodes::RPCFailed;
      rsp["message"] = "RPC 失败: " + status.error_message();
    }
  } catch (const std::exception &e) {
    rsp["error"] = ErrorCodes::InternalError;
    rsp["message"] = "系统异常: " + std::string(e.what());
  }

  return rsp;
}

Json::Value FileSend(ChatSession::Ptr session, uint32_t msgID,
                     Json::Value &request) {
  /* 一面推送消息，一面存储消息，其中离线情况，
  message表中的content对文件消息而言无作用，因其存储在文件系统
  存储规则目前暂为：fileService/uid/[seq].txt
*/
  return {};
}

Json::Value TextSend(ChatSession::Ptr session, uint32_t msgID,
                     Json::Value &request) {

  Json::Value rsp;

  int64_t clientSeq = request["seq"].asInt64();
  int64_t from = request["from"].asInt64();
  int64_t to = request["to"].asInt64();
  int64_t sessionHashKey = request["sessionKey"].asInt64();
  std::string data = request["data"].asString();

  // 发送者回应包中包含它的消息序列号，表示已接收并处理了请求
  rsp["seq"] = Json::Value::Int64(clientSeq);

  bool missMessageCache =
      db::RedisDao::GetInstance()->getUserMsgId(from, clientSeq);
  if (missMessageCache) {
    LOG_INFO(businessLogger, "重复消息, 客户端消息序列号为: {}", clientSeq);
    db::RedisDao::GetInstance()->expireUserMsgId(
        from, clientSeq, MESSAGE_CACHE_EXPIRE_TIME_SECONDS);
    rsp["error"] = ErrorCodes::RepeatMessage;
    rsp["message"] = "重复消息";
    return rsp;
  }

  std::string userInfo = db::RedisDao::GetInstance()->getOnlineUserInfo(to);
  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  bool isOtherMachineOnline =
      (userInfo.empty() == false && isLocalMachineOnline == false);
  if (isOtherMachineOnline) {
    LOG_INFO(businessLogger, "用户不在本地机器，转发消息到其他机器, 用户ID: {}",
             to);
    rpc::TextSendMessageRequest rpcRequest;
    rpc::TextSendMessageResponse rpcResponse;
    rpcRequest.set_from(from);
    rpcRequest.set_to(to);
    rpcRequest.set_text(data);

    rpcResponse = rpc::ImRpc::GetInstance()
                      ->getRpc(sessionHashKey)
                      ->forwardTextSendMessage(rpcRequest);

    std::string status = rpcResponse.status();
    LOG_INFO(businessLogger, "转发成功，rpc 响应: {}, 状态码: {}",
             rpcResponse.DebugString(), status);
    rsp["status"] = "wait";
    if (status != "success")
      rsp["error"] = ErrorCodes::RPCFailed;
    else
      rsp["error"] = ErrorCodes::Success;
    return rsp;
  }

  int64_t serverMsgSeq = 0;
  db::RedisDao::GetInstance()->setUserMsgId(from, clientSeq,
                                            MESSAGE_CACHE_EXPIRE_TIME_SECONDS);
  serverMsgSeq = db::RedisDao::GetInstance()->generateMsgId();

  if (isLocalMachineOnline) {
    LOG_INFO(businessLogger, "用户本地在线，发送消息给目标用户，ID: {}", to);
    rsp["status"] = "wait";
    rsp["error"] = ErrorCodes::Success;

    // 替换成服务端消息序列号，因需维护服务端道接收者的消息查收状态
    request["seq"] = Json::Value::Int64(serverMsgSeq);
    OnlineUser::GetInstance()->onReWrite(
        OnlineUser::ReWriteType::Message, serverMsgSeq, to,
        request.toStyledString(), ID_TEXT_SEND_REQ);

    return rsp;
  }

  LOG_INFO(businessLogger, "用户不在线，存储离线消息, 用户ID: {}", to);
  db::Message::Ptr message = nullptr;

  message.reset(new db::Message(
      serverMsgSeq, from, to, std::to_string(sessionHashKey),
      db::Message::Type::TEXT, data, db::Message::Status::WAIT));

  int sqlStatus = db::MysqlDao::GetInstance()->insertMessage(message);
  rsp["status"] = "wait";
  if (sqlStatus != -1)
    rsp["error"] = ErrorCodes::Success;
  else
    rsp["error"] = ErrorCodes::MysqlFailed;

  return rsp;
}

Json::Value UserQuit(ChatSession::Ptr session, uint32_t msgID,
                     Json::Value &request) {
  auto uid = request["uid"].asInt64();

  /*
  清理在线资源，网络层资源在对方close关闭时自行清理
  每个用户都有心跳机制，此处默认清理
  */
  OnlineUser::GetInstance()->ClearUser(uid, uid);
  return {};
} // namespace wim

Json::Value ReLogin(int64_t uid, ChatSession::Ptr oldSession,
                    ChatSession::Ptr newSession) {
  Json::Value rsp;

  return rsp;
}

Json::Value OnLogin(ChatSession::Ptr session, uint32_t msgID,
                    Json::Value &request) {
  Json::Value rsp;
  int64_t uid = request["uid"].asInt64();
  bool isFirstLogin = request["init"].asBool();
  int status = 0;

  // 待实现，先不做处理
  status = OnlineUser::GetInstance()->isOnline(uid);
  if (false && status == false) {

    rsp["error"] = ErrorCodes::UserOnline;
    auto oldSession = OnlineUser::GetInstance()->GetUserSession(uid);
    ReLogin(uid, oldSession, session);
  }

  status = db::RedisDao::GetInstance()->getOnlineUserInfo(uid).empty();
  // 分布式情况，待实现
  if (false && status) {
    rsp["error"] = ErrorCodes::Success;
  }

  // 用户信息处理
  db::UserInfo::Ptr userInfo;
  if (isFirstLogin) {
    // 首次登录，需要同步用户信息
    std::string name = request["name"].asString();
    short age = request["age"].asInt();
    std::string sex = request["sex"].asString();
    userInfo.reset(new db::UserInfo(uid, name, age, sex, {}));

    status = db::MysqlDao::GetInstance()->insertUserInfo(userInfo);
    if (status != 0) {
      LOG_WARN(wim::businessLogger, "插入用户信息失败, uid-{} ", uid);
      rsp["error"] = -1;
      return rsp;
    }
  } else {
    userInfo = db::MysqlDao::GetInstance()->getUserInfo(uid);
    if (userInfo == nullptr) {
      LOG_WARN(wim::businessLogger, "获取用户信息失败, uid-{} ", uid);
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

Json::Value pullFriendApplyList(ChatSession::Ptr session, uint32_t msgID,
                                Json::Value &request) {
  Json::Value rsp;

  int64_t uid = request["uid"].asInt64();
  db::FriendApply::FriendApplyGroup applyList =
      db::MysqlDao::GetInstance()->getFriendApplyList(uid);
  if (applyList == nullptr) {
    LOG_INFO(businessLogger, "回应表为空, uid: {}", uid);
    rsp["error"] = ErrorCodes::Success;
    rsp["applyList"] = Json::Value(Json::arrayValue);
    return rsp;
  }

  for (auto applyObject : *applyList) {
    Json::Value root;
    root["from"] = Json::Value::Int64(applyObject->from);
    root["to"] = Json::Value::Int64(applyObject->to);
    root["status"] = applyObject->status;
    root["content"] = applyObject->content;
    root["applyDateTime"] = applyObject->createTime;
    rsp["applyList"].append(root);
  }
  rsp["error"] = ErrorCodes::Success;

  return rsp;
}

Json::Value pullFriendList(ChatSession::Ptr session, uint32_t msgID,
                           Json::Value &request) {
  Json::Value rsp;

  int64_t uid = request["uid"].asInt64();
  db::Friend::FriendGroup friendList =
      db::MysqlDao::GetInstance()->getFriendList(uid);

  if (friendList == nullptr) {
    LOG_INFO(wim::businessLogger, "好友表为空, uid-{}", uid);
    rsp["error"] = ErrorCodes::Success;
    rsp["friendList"] = Json::Value(Json::arrayValue);
    return rsp;
  }

  for (auto friendObject : *friendList) {
    int64_t friendUid = friendObject->uidB;
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
Json::Value pullSessionMessageList(ChatSession::Ptr session, uint32_t msgID,
                                   Json::Value &request) {
  Json::Value rsp;

  int64_t from = request["from"].asInt64();
  int64_t to = request["to"].asInt64();
  int64_t lastMsgId = request["lastMsgId"].asInt64();
  int limit = request["limit"].asInt();

  auto messageList = db::MysqlDao::GetInstance()->getSessionMessage(
      from, to, lastMsgId, limit);
  if (messageList == nullptr) {
    LOG_INFO(wim::businessLogger,
             "消息表为空, from: {}, to: {}, lastMsgId: {}, limit: {}", from, to,
             lastMsgId, limit);

    rsp["error"] = ErrorCodes::Success;
    rsp["messageList"] = Json::Value(Json::arrayValue);
    return rsp;
  }

  rsp["uid"] = Json::Value::Int64(to);
  for (auto message : *messageList) {
    Json::Value tmp;
    tmp["messageId"] = Json::Value::Int64(message->messageId);
    tmp["type"] = message->type;
    tmp["content"] = message->content;
    tmp["status"] = message->status;
    tmp["sendDateTime"] = message->sendDateTime;
    tmp["readDateTime"] = message->readDateTime;

    rsp["messageList"].append(tmp);
  }
  rsp["error"] = ErrorCodes::Success;

  return rsp;
}

Json::Value pullMessageList(ChatSession::Ptr session, uint32_t msgID,
                            Json::Value &request) {
  Json::Value rsp;

  int64_t uid = request["uid"].asInt64();
  int64_t lastMsgId = request["lastMsgId"].asInt64();
  int limit = request["limit"].asInt();

  rsp["uid"] = Json::Value::Int64(uid);

  auto messageList =
      db::MysqlDao::GetInstance()->getUserMessage(uid, lastMsgId, limit);
  if (messageList == nullptr) {
    LOG_INFO(wim::businessLogger,
             "消息表为空,  uid: {}, lastMsgId: {}, limit: {}", uid, lastMsgId,
             limit);

    rsp["error"] = ErrorCodes::Success;
    rsp["messageList"] = Json::Value(Json::arrayValue);
    return rsp;
  }

  for (auto message : *messageList) {
    Json::Value tmp;
    tmp["messageId"] = Json::Value::Int64(message->messageId);
    tmp["type"] = message->type;
    tmp["content"] = message->content;
    tmp["status"] = message->status;
    tmp["sendDateTime"] = message->sendDateTime;
    tmp["readDateTime"] = message->readDateTime;

    rsp["messageList"].append(tmp);
  }
  rsp["error"] = ErrorCodes::Success;

  return rsp;
}
}; // namespace wim
