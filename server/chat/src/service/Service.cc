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
#include <fcntl.h>
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
  2、转发处理时，先单向的发送请求响应包，再通过rpc转发间接响应目标接收者
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
    int responseId = __getServiceResponseId(ServiceID(id));

    auto handleCaller = serviceGroup.find(id);
    if (handleCaller == serviceGroup.end()) {
      LOG_INFO(wim::businessLogger, "没有这样的服务，ID： {}, ", id);

      TcpPacket rsp = MakeErrorPacket(ErrorCodes::NotFound);
      channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
      messageQueue.pop();
      return;
    }

    std::string msgData{data, dataSize};
    TcpPacket request, response;
    std::string requestIdMessage = getServiceIdString(id),
                responseIdMessage = getServiceIdString(responseId);

    bool parserSuccess = ParseTcpPacket(msgData, request);
    if (!parserSuccess) {
      LOG_WARN(wim::businessLogger, "消息解析失败, 请求服务: {}，消息：{}",
               requestIdMessage, msgData);
      TcpPacket rsp = MakeErrorPacket(ErrorCodes::JsonParser);
      channel->contextSession->Send(SerializeTcpPacket(rsp), responseId);
      messageQueue.pop();
      return;
    }
    LOG_DEBUG(wim::businessLogger, "解析成功，请求服务: {}, 请求数据: {}",
              requestIdMessage, TcpPacketDebugString(request));

    response = handleCaller->second(channel->contextSession, id, request);
    if (id == ID_ACK) {
      LOG_DEBUG(wim::businessLogger, "ACK已处理，不发送响应包: {}",
                TcpPacketDebugString(response));
      messageQueue.pop();
      return;
    }
    auto ret = SerializeTcpPacket(response);
    channel->contextSession->Send(ret, responseId);

    LOG_DEBUG(wim::businessLogger, "响应服务: {}, 响应数据: {}",
              responseIdMessage, TcpPacketDebugString(response));

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

TcpPacket PingHandle(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request) {
  TcpPacket rsp;
  int64_t uid = request.uid();

  rsp.set_uid(uid);
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

TcpPacket AckHandle(ChatSession::Ptr session, uint32_t msgID,
                    TcpPacket &request) {
  TcpPacket rsp;
  int64_t seq = request.seq();
  int64_t uid = request.uid();

  OnlineUser::GetInstance()->cancelAckTimer(seq, uid);
  db::MysqlDao::GetInstance()->updateMessage(seq, db::Message::Status::DONE,
                                             getCurrentDateTime());
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

TcpPacket SerachUser(ChatSession::Ptr session, uint32_t msgID,
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

TcpPacket UploadFile(ChatSession::Ptr session, uint32_t msgID,
                     TcpPacket &request) {
  TcpPacket rsp;
  int64_t clientSeq = request.seq();
  int64_t uid = request.uid();
  std::string data = request.data();
  std::string fileName = request.file_name();
  rpc::FileType type;

  rsp.set_seq(clientSeq);

  bool hasUserMsgId = db::RedisDao::GetInstance()->getUserMsgId(uid, clientSeq);
  static short __expireUserMsgId = 10;
  if (hasUserMsgId) {
    LOG_INFO(businessLogger, "重复消息, 客户端消息序列号为: {}", clientSeq);
    db::RedisDao::GetInstance()->expireUserMsgId(uid, clientSeq,
                                                 __expireUserMsgId);
    rsp.set_error(ErrorCodes::RepeatMessage);
    rsp.set_message("重复消息");
    return rsp;
  }

  std::string tmpType = request.file_type();
  if (tmpType == "TEXT") {
    type = rpc::FileType::TEXT;
  } else if (tmpType == "IMAGE") {
    type = rpc::FileType::IMAGE;
  } else {
    LOG_ERROR(businessLogger, "文件传输错误");
    rsp.set_error(ErrorCodes::FileTypeError);
    rsp.set_message("文件类型错误");
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
      rsp.set_error(ErrorCodes::Success);
    } else {
      rsp.set_error(ErrorCodes::RPCFailed);
      rsp.set_message("RPC 失败: " + status.error_message());
    }
  } catch (const std::exception &e) {
    rsp.set_error(ErrorCodes::InternalError);
    rsp.set_message("系统异常: " + std::string(e.what()));
  }

  return rsp;
}

TcpPacket FileSend(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request) {
  /* 一面推送消息，一面存储消息，其中离线情况，
  message表中的content对文件消息而言无作用，因其存储在文件系统
  存储规则目前暂为：fileService/uid/[seq].txt
*/
  return {};
}

TcpPacket TextSend(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request) {
  TcpPacket rsp;

  int64_t clientSeq = request.seq();
  int64_t from = request.from();
  int64_t to = request.to();
  int64_t sessionHashKey = request.session_key();
  std::string data = request.data();

  // 发送者回应包中包含它的消息序列号，表示已接收并处理了请求
  rsp.set_seq(clientSeq);

  bool missMessageCache =
      db::RedisDao::GetInstance()->getUserMsgId(from, clientSeq);
  if (missMessageCache) {
    LOG_INFO(businessLogger, "重复消息, 客户端消息序列号为: {}", clientSeq);
    db::RedisDao::GetInstance()->expireUserMsgId(
        from, clientSeq, MESSAGE_CACHE_EXPIRE_TIME_SECONDS);
    rsp.set_error(ErrorCodes::RepeatMessage);
    rsp.set_message("重复消息");
    return rsp;
  }

  Json::Value userInfo =
      db::RedisDao::GetInstance()->getOnlineUserInfoObject(to);
  bool isLocalMachineOnline = OnlineUser::GetInstance()->isOnline(to);
  bool isOtherMachineOnline =
      (!userInfo.empty() && isLocalMachineOnline == false);
  if (isOtherMachineOnline) {
    LOG_INFO(businessLogger, "用户不在本地机器，转发消息到其他机器, 用户ID: {}",
             to);
    std::string machineId = userInfo["machineId"].asString();
    rpc::TextSendMessageRequest rpcRequest;
    rpc::TextSendMessageResponse rpcResponse;
    rpcRequest.set_from(from);
    rpcRequest.set_to(to);
    rpcRequest.set_text(data);
    rpcRequest.set_seq(clientSeq);
    rpcRequest.set_session_key(sessionHashKey);

    auto rpcNode = rpc::ImRpc::GetInstance()->getRpc(machineId);
    if (rpcNode == nullptr) {
      LOG_WARN(businessLogger, "未找到目标IM机器的RPC连接, machineId: {}",
               machineId);
      rsp.set_status("wait");
      rsp.set_error(ErrorCodes::RPCFailed);
      rsp.set_message("目标IM机器不可达");
      return rsp;
    }

    rpcResponse = rpcNode->forwardTextSendMessage(rpcRequest);

    std::string status = rpcResponse.status();
    LOG_INFO(businessLogger, "转发完成，目标机器: {}, rpc 响应: {}, 状态码: {}",
             machineId, rpcResponse.DebugString(), status);
    rsp.set_status("wait");
    if (status != "success")
      rsp.set_error(ErrorCodes::RPCFailed);
    else
      rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  int64_t serverMsgSeq = 0;
  db::RedisDao::GetInstance()->setUserMsgId(from, clientSeq,
                                            MESSAGE_CACHE_EXPIRE_TIME_SECONDS);
  serverMsgSeq = db::RedisDao::GetInstance()->generateMsgId();
  const std::string sendTime = getCurrentDateTime();

  if (isLocalMachineOnline) {
    LOG_INFO(businessLogger, "用户本地在线，发送消息给目标用户，ID: {}", to);
    db::Message::Ptr message(new db::Message(
        serverMsgSeq, from, to, std::to_string(sessionHashKey),
        db::Message::Type::TEXT, data, db::Message::Status::WAIT, sendTime));
    int sqlStatus = db::MysqlDao::GetInstance()->insertMessage(message);
    if (sqlStatus == -1) {
      rsp.set_error(ErrorCodes::MysqlFailed);
      rsp.set_message("消息落库失败");
      return rsp;
    }

    rsp.set_status("wait");
    rsp.set_error(ErrorCodes::Success);

    // 替换成服务端消息序列号，因需维护服务端道接收者的消息查收状态
    request.set_seq(serverMsgSeq);
    OnlineUser::GetInstance()->onReWrite(
        OnlineUser::ReWriteType::Message, serverMsgSeq, to,
        SerializeTcpPacket(request), ID_TEXT_SEND_REQ);

    return rsp;
  }

  LOG_INFO(businessLogger, "用户不在线，存储离线消息, 用户ID: {}", to);
  db::Message::Ptr message = nullptr;

  message.reset(new db::Message(
      serverMsgSeq, from, to, std::to_string(sessionHashKey),
      db::Message::Type::TEXT, data, db::Message::Status::WAIT, sendTime));

  int sqlStatus = db::MysqlDao::GetInstance()->insertMessage(message);
  rsp.set_status("wait");
  if (sqlStatus != -1)
    rsp.set_error(ErrorCodes::Success);
  else
    rsp.set_error(ErrorCodes::MysqlFailed);

  return rsp;
}

TcpPacket UserQuit(ChatSession::Ptr session, uint32_t msgID,
                   TcpPacket &request) {
  auto uid = request.uid();

  /*
  清理在线资源，网络层资源在对方close关闭时自行清理
  每个用户都有心跳机制，此处默认清理
  */
  OnlineUser::GetInstance()->ClearUser(uid, uid, session);
  return {};
}  // namespace wim

TcpPacket ReLogin(int64_t uid, ChatSession::Ptr oldSession,
                  ChatSession::Ptr newSession) {
  TcpPacket rsp;

  return rsp;
}

TcpPacket OnLogin(ChatSession::Ptr session, uint32_t msgID,
                  TcpPacket &request) {
  TcpPacket rsp;
  int64_t uid = request.uid();
  bool isFirstLogin = request.init();
  int status = 0;

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
  session->SetUserId(uid);

  rsp.set_uid(userInfo->uid);
  rsp.set_name(userInfo->name);
  rsp.set_age(userInfo->age);
  rsp.set_sex(userInfo->sex);
  rsp.set_head_image_url(userInfo->headImageURL);
  rsp.set_error(ErrorCodes::Success);
  return rsp;
}

TcpPacket pullFriendApplyList(ChatSession::Ptr session, uint32_t msgID,
                              TcpPacket &request) {
  TcpPacket rsp;

  int64_t uid = request.uid();
  db::FriendApply::FriendApplyGroup applyList =
      db::MysqlDao::GetInstance()->getFriendApplyList(uid);
  if (applyList == nullptr) {
    LOG_INFO(businessLogger, "回应表为空, uid: {}", uid);
    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  for (auto applyObject : *applyList) {
    auto *apply = rsp.add_apply_list();
    apply->set_from(applyObject->from);
    apply->set_to(applyObject->to);
    apply->set_status(applyObject->status);
    apply->set_content(applyObject->content);
    apply->set_apply_date_time(applyObject->createTime);
  }
  rsp.set_error(ErrorCodes::Success);

  return rsp;
}

TcpPacket pullFriendList(ChatSession::Ptr session, uint32_t msgID,
                         TcpPacket &request) {
  TcpPacket rsp;

  int64_t uid = request.uid();
  db::Friend::FriendGroup friendList =
      db::MysqlDao::GetInstance()->getFriendList(uid);

  if (friendList == nullptr) {
    LOG_INFO(wim::businessLogger, "好友表为空, uid-{}", uid);
    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  for (auto friendObject : *friendList) {
    int64_t friendUid = friendObject->uidB;
    db::UserInfo::Ptr friendInfo =
        db::MysqlDao::GetInstance()->getUserInfo(friendUid);
    if (friendInfo != nullptr) {
      auto *info = rsp.add_friend_list();
      info->set_uid(friendUid);
      info->set_name(friendInfo->name);
      info->set_age(friendInfo->age);
      info->set_sex(friendInfo->sex);
      info->set_head_image_url(friendInfo->headImageURL);
    }
  }

  rsp.set_error(ErrorCodes::Success);
  return rsp;
}
TcpPacket pullSessionMessageList(ChatSession::Ptr session, uint32_t msgID,
                                 TcpPacket &request) {
  TcpPacket rsp;

  int64_t from = request.from();
  int64_t to = request.to();
  int64_t lastMsgId = request.last_msg_id();
  int limit = request.limit();

  auto messageList = db::MysqlDao::GetInstance()->getSessionMessage(
      from, to, lastMsgId, limit);
  if (messageList == nullptr) {
    LOG_INFO(wim::businessLogger,
             "消息表为空, from: {}, to: {}, lastMsgId: {}, limit: {}", from, to,
             lastMsgId, limit);

    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  rsp.set_uid(to);
  for (auto message : *messageList) {
    auto *item = rsp.add_message_list();
    item->set_message_id(message->messageId);
    item->set_type(message->type);
    item->set_content(message->content);
    item->set_status(message->status);
    item->set_send_date_time(message->sendDateTime);
    item->set_read_date_time(message->readDateTime);
  }
  rsp.set_error(ErrorCodes::Success);

  return rsp;
}

TcpPacket pullMessageList(ChatSession::Ptr session, uint32_t msgID,
                          TcpPacket &request) {
  TcpPacket rsp;

  int64_t uid = request.uid();
  int64_t lastMsgId = request.last_msg_id();
  int limit = request.limit();

  rsp.set_uid(uid);

  auto messageList =
      db::MysqlDao::GetInstance()->getUserMessage(uid, lastMsgId, limit);
  if (messageList == nullptr) {
    LOG_INFO(wim::businessLogger,
             "消息表为空,  uid: {}, lastMsgId: {}, limit: {}", uid, lastMsgId,
             limit);

    rsp.set_error(ErrorCodes::Success);
    return rsp;
  }

  for (auto message : *messageList) {
    auto *item = rsp.add_message_list();
    item->set_message_id(message->messageId);
    item->set_type(message->type);
    item->set_content(message->content);
    item->set_status(message->status);
    item->set_send_date_time(message->sendDateTime);
    item->set_read_date_time(message->readDateTime);
  }
  rsp.set_error(ErrorCodes::Success);

  return rsp;
}
};  // namespace wim
