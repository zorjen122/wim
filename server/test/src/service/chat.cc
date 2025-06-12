#include "chat.h"
#include "Configer.h"
#include "Const.h"
#include "Logger.h"
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/value.h>

namespace wim {

static std::shared_ptr<net::steady_timer> waitPongTimer;
static std::map<long, std::shared_ptr<net::steady_timer>> waitAckTimerMap;

static long generateRandomId() {
  static std::random_device rd;  // 使用硬件随机数生成器
  static std::mt19937 gen(rd()); // Mersenne Twister 作为随机数引擎
  static std::uniform_int_distribution<long> dis(1000000000,
                                                 9999999999); // 随机数范围

  return dis(gen); // 返回一个随机数
}
void Chat::nullHandle(const Json::Value &response) {}

Chat::Chat() {
  using namespace std::placeholders;
  handleMap[ID_LOGIN_INIT_RSP] = std::bind(&Chat::loginInitHandle, this, _1);
  handleMap[ID_PING_RSP] = std::bind(&Chat::pingHandle, this, _1);

  handleMap[ID_NOTIFY_ADD_FRIEND_RSP] =
      std::bind(&Chat::notifyAddFriendSenderHandle, this, _1);
  handleMap[ID_NOTIFY_ADD_FRIEND_REQ] =
      std::bind(&Chat::notifyAddFriendRecvierHandle, this, _1);

  handleMap[ID_REPLY_ADD_FRIEND_RSP] =
      std::bind(&Chat::replyAddFriendSenderHandle, this, _1);
  handleMap[ID_REPLY_ADD_FRIEND_REQ] =
      std::bind(&Chat::replyAddFriendRecvierHandle, this, _1);

  handleMap[ID_TEXT_SEND_RSP] = std::bind(&Chat::textSenderHandle, this, _1);
  handleMap[ID_TEXT_SEND_REQ] = std::bind(&Chat::textRecvierHandle, this, _1);

  handleMap[ID_FILE_UPLOAD_RSP] = std::bind(&Chat::uploadFileHandle, this, _1);

  // 待测试
  handleMap[ID_SEARCH_USER_RSP] = std::bind(&Chat::serachUserHandle, this, _1);

  handleMap[ID_PULL_FRIEND_LIST_RSP] =
      std::bind(&Chat::pullFriendListHandle, this, _1);
  handleMap[ID_PULL_FRIEND_APPLY_LIST_RSP] =
      std::bind(&Chat::pullFriendApplyListHandle, this, _1);
  handleMap[ID_PULL_SESSION_MESSAGE_LIST_RSP] =
      std::bind(&Chat::pullMessageListHandle, this, _1);

  handleMap[ID_PULL_MESSAGE_LIST_RSP] =
      std::bind(&Chat::pullMessageListHandle, this, _1);

  handleMap[ID_INIT_USER_INFO_REQ] =
      std::bind(&Chat::initUserInfoHandle, this, _1);

  handleMap[ID_GROUP_CREATE_RSP] =
      std::bind(&Chat::createGroupHandle, this, _1);

  handleMap[ID_GROUP_NOTIFY_JOIN_RSP] =
      std::bind(&Chat::joinGroupSenderHandle, this, _1);
  handleMap[ID_GROUP_NOTIFY_JOIN_REQ] =
      std::bind(&Chat::joinGroupRecvierHandle, this, _1);

  handleMap[ID_GROUP_REPLY_JOIN_RSP] =
      std::bind(&Chat::replyAddFriendSenderHandle, this, _1);
  handleMap[ID_GROUP_REPLY_JOIN_REQ] =
      std::bind(&Chat::replyAddFriendRecvierHandle, this, _1);

  handleMap[ID_GROUP_TEXT_SEND_RSP] =
      std::bind(&Chat::textSenderHandle, this, _1);
  handleMap[ID_GROUP_TEXT_SEND_REQ] =
      std::bind(&Chat::textRecvierHandle, this, _1);

  handleMap[ID_NULL] = std::bind(&Chat::nullHandle, this, _1);
}

// 暂行，待测试
void File_recv_rsp_handler(int from, int to, long seq, std::string text) {
  std::string filePath = Configer::getSaveFilePath() + std::to_string(from) +
                         "/" + std::to_string(seq) + ".txt";
  std::ofstream ofs(filePath, std::ios::binary | std::ios::app);
  ofs.write(text.data(), text.size());
  ofs.close();
}

void Chat::replyAddFriendSenderHandle(const Json::Value &response) {
  auto fromUid = response["uid"].asInt64();
  auto accept = response["accept"].asBool();
  auto replyMessage = response["replyMessage"].asString();
  if (friendApplyMap.find(fromUid) == friendApplyMap.end()) {
    spdlog::error("好友申请回应信息未保存到本地, uid:{}", fromUid);
    return;
  }
  if (response["error"].asInt() == ErrorCodes::Success) {
    friendApplyMap[fromUid]->status = accept;
    friendApplyMap[fromUid]->content = replyMessage;
  }
}

void Chat::replyAddFriendRecvierHandle(const Json::Value &response) {
  long uid = response["uid"].asInt64();
  long seq = response["seq"].asInt64();

  bool missCache = CheckAckCache(seq);
  if (missCache)
    return;
  Json::Value ack;
  ack["seq"] = response["seq"];
  ack["uid"] = Json::Value::Int64(user->uid);
  chat->Send(ack.toStyledString(), ID_ACK);

  // 服务端处理失败时则回复原来状态
  if (response["error"].asInt() != ErrorCodes::Success) {
    LOG_WARN(businessLogger, "答复好友申请出现了异常, message: {}",
             response["message"].asString());
    if (friendApplyMap.find(uid) == friendApplyMap.end()) {
      LOG_INFO(businessLogger, "friend apply not found, uid:{}", uid);
      return;
    }
    friendApplyMap[uid]->status = 0;
    if (friendMap.find(uid) != friendMap.end()) {
      LOG_INFO(businessLogger, "未在本地找到好友申请信息, uid:{}", uid);
    }
    friendMap.erase(uid);
    return;
  }
}

void Chat::notifyAddFriendSenderHandle(const Json::Value &response) {
  long uid = response["uid"].asInt64();

  // 服务端处理失败时则回复原来状态
  if (response["error"].asInt() != ErrorCodes::Success) {
    LOG_WARN(businessLogger, "申请好友失败, message: {}",
             response["message"].asString());
    if (friendApplyMap.find(uid) == friendApplyMap.end()) {
      LOG_INFO(businessLogger, "申请好友信息未保存到本地, uid:{}", uid);
      return;
    }
    friendApplyMap.erase(uid);
    return;
  }
}
void Chat::notifyAddFriendRecvierHandle(const Json::Value &response) {

  db::FriendApply::Ptr apply(new db::FriendApply());
  apply->from = response["from"].asInt64();
  apply->content = response["content"].asString();
  apply->to = user->uid;
  apply->status = 0;
  friendApplyMap[apply->from] = apply;
}
void Chat::loginInitHandle(const Json::Value &response) {
  auto errcode = response["error"].asInt();
  if (errcode == -1) {
    LOG_INFO(businessLogger, "登录失败，请重试");
    return;
  }

  std::string name = response["name"].asString();
  short age = response["age"].asInt();
  std::string sex = response["sex"].asString();
  std::string headImageURL = response["headImageURL"].asString();
}
void Chat::textSenderHandle(const Json::Value &response) {
  long seq = response["seq"].asInt64();
  auto timer = waitAckTimerMap[seq];
  if (timer == nullptr) {
    LOG_INFO(businessLogger,
             "未找到序列号对应的重发定时器，或是未开启超时重传、或是发生异常, "
             "序列号: {}",
             seq);
    return;
  }
  timer->cancel();
  LOG_INFO(businessLogger, "消息成功被接收，序列号为：{}", seq);
}
bool Chat::CheckAckCache(int64_t seq) {

  Json::Value ack;
  ack["seq"] = Json::Value::Int64(seq);
  ack["uid"] = Json::Value::Int64(user->uid);
  bool missCache = seqCacheExpireMap.find(seq) != seqCacheExpireMap.end();
  // 若ACK已被服务端收到，则意味着其不会重发，反之则重发，若其重发，则复发一次ACK给客户端
  if (missCache) {
    chat->Send(ack.toStyledString(), ID_ACK);
    return true;
  }

  seqCacheExpireMap[seq] = std::make_shared<net::steady_timer>(
      chat->iocontext, std::chrono::seconds(5));
  seqCacheExpireMap[seq]->async_wait(
      [this, seq](const boost::system::error_code &ec) {
        if (!ec) {
          seqCacheExpireMap.erase(seq);
          LOG_INFO(businessLogger, "消息序列号缓存（{}）过期", seq);
        } else if (ec == boost::asio::error::operation_aborted) {
          LOG_INFO(businessLogger, "消息序列号（{}）被取消", seq);
        }
      });
  return false;
}

void Chat::textRecvierHandle(const Json::Value &response) {
  long from = response["from"].asInt64();
  long to = response["to"].asInt64();
  long seq = response["seq"].asInt64();
  std::string data = response["data"].asString();
  int type = response["type"].asInt();

  bool missCache = CheckAckCache(seq);
  if (missCache)
    return;

  // 推入到消息队列中，每隔100ms拉取（确保%99有序————具体保障P99的延迟数值待分析)
  using db::Message;
  Message message;
  message.messageId = seq;
  message.from = from;
  message.to = to;

  message.content = data;
  messageQueue.push_back(message);

  bool isFirst = messageReadTimer == nullptr;
  if (isFirst) {
    messageReadTimer = std::make_shared<net::steady_timer>(
        chat->iocontext, std::chrono::seconds(10));
  }
  // 查看定时器是否已启动，消费消息的触发逻辑放在handleRun中，则是惰性的，以避免重复启动定时器
  bool onRunMessageTimer =
      messageReadTimer->expiry() > boost::asio::steady_timer::clock_type::now();
  if (isFirst || onRunMessageTimer == false) {
    messageReadTimer->async_wait([this](const boost::system::error_code &ec) {
      if (ec == boost::asio::error::operation_aborted) {
        LOG_INFO(businessLogger, "messageReadTimer canceled");
      } else {
        std::lock_guard<std::mutex> lock(comsumeMessageMutex);
        std::sort(messageQueue.begin(), messageQueue.end(),
                  [](const Message &a, const Message &b) {
                    return a.messageId < b.messageId;
                  });
        std::cout << "已排序消息，序列号为（组）：[\n";
        for (auto &message : messageQueue) {
          std::cout << message.messageId << " : "
                    << "[来自用户：" << message.from << "]: " << message.content
                    << "\n";
        }
        std::cout << "\n]\n";
        messageQueue.clear();
      }
    });
  }

  Json::Value ack = response["seq"];
  ack["uid"] = Json::Value::Int64(user->uid);
  chat->Send(ack.toStyledString(), ID_ACK);
}
void Chat::uploadFileHandle(const Json::Value &response) {}

void Chat::handleRun(Tlv::Ptr protocolData) {

  Json::Value response;
  Json::Reader reader;
  reader.parse(protocolData->getDataString(), response);
  auto errcode = response["error"].asInt();
  if (errcode == -1) {
    LOG_WARN(businessLogger, "响应异常： {}", response.toStyledString());
  }

  auto handleFunc = handleMap.find(protocolData->id);

  if (handleFunc == handleMap.end()) {
    LOG_WARN(businessLogger, "没有这样的服务响应函数, 响应码: {}, response: {}",
             protocolData->id, response.toStyledString());
    return;
  }

  LOG_DEBUG(wim::businessLogger, "响应服务: {}, 响应包: {}",
            getServiceIdString(protocolData->id), response.toStyledString());
  handleFunc->second(response);
}

void Chat::initUserInfo(db::UserInfo::Ptr userInfo) {
  Json::Value request;
  request["name"] = userInfo->name;
  request["age"] = userInfo->age;
  request["sex"] = userInfo->sex;
  chat->Send(request.toStyledString(), ID_INIT_USER_INFO_REQ);
}

void Chat::initUserInfoHandle(const Json::Value &response) {
  // pull...
}
bool Chat::login(bool isFirstLogin) {

  Json::Value loginRequest;
  loginRequest["uid"] = Json::Value::Int64(user->uid);

  chat->Send(loginRequest.toStyledString(), ID_LOGIN_INIT_REQ);
  pullFriendList();
  pullFriendApplyList();
  pullMessageList();
  return true;
}

void Chat::quit() {
  Json::Value quitRequest;
  quitRequest["uid"] = Json::Value::Int64(user->uid);
  chat->Send(quitRequest.toStyledString(), ID_USER_QUIT_REQ);
}

bool Chat::ping() {
  Json::Value ping;
  ping["uid"] = Json::Value::Int64(user->uid);

  std::string pingBuffer = ping.toStyledString();
  // LOG_INFO(businessLogger, "request json: {}", pingBuffer);
  chat->Send(pingBuffer, ID_PING_REQ);
  return true;
}

bool Chat::OnheartBeat(int count) {

  waitPongTimer.reset(new net::steady_timer(chat->getIoContext()));
  waitPongTimer->expires_after(std::chrono::seconds(2));
  waitPongTimer->async_wait([count, this](boost::system::error_code ec) {
    if (ec == boost::asio::error::operation_aborted) {
      // LOG_INFO(businessLogger,"onRePongWrite timer canceled | uid-{}",
      // user->uid);
      return;
    } else {
      // 暂行方案，可进一步考虑租约机制，未超时采用n秒租约以免于频繁Ping，若超时一次后则采用默认心跳
      static const int max_retry_count = 3;
      if (count + 1 > max_retry_count) {
        arrhythmiaHandle(user->uid);
        return;
      }

      ping();
      LOG_INFO(businessLogger, "心跳超时，当前重试次数: {}", count);
      this->OnheartBeat(count + 1);
      return;
    }
  });
  return true;
}
void Chat::pingHandle(const Json::Value &response) {
  if (waitPongTimer == nullptr) {
    spdlog::warn("心跳定时器为空");
    return;
  }

  waitPongTimer->cancel();
  OnheartBeat(0);
}
void Chat::arrhythmiaHandle(long uid) {
  LOG_INFO(businessLogger, "arrhythmiaHandle: uid-{}", uid);
}

void Chat::serachUserHandle(const Json::Value &response) {

  long uid = response["uid"].asInt64();
  std::string name = response["name"].asString();
  short age = response["age"].asInt();
  short sex = response["sex"].asInt();
  std::string headImageURL = response["headImageURL"].asString();
}

bool Chat::searchUser(const std::string &username) {
  Json::Value searchReq;
  searchReq["username"] = username;
  chat->Send(searchReq.toStyledString(), ID_SEARCH_USER_REQ);

  LOG_INFO(wim::businessLogger, "search user: {}", username);
  return true;
}

bool Chat::pullFriendList() {
  Json::Value request;
  request["uid"] = Json::Value::Int64(user->uid);
  chat->Send(request.toStyledString(), ID_PULL_FRIEND_LIST_REQ);
  return true;
}
bool Chat::pullFriendApplyList() {
  Json::Value request;
  request["uid"] = Json::Value::Int64(user->uid);
  chat->Send(request.toStyledString(), ID_PULL_FRIEND_APPLY_LIST_REQ);
  return true;
}

bool Chat::pullMessageList() {
  Json::Value request;
  request["uid"] = Json::Value::Int64(user->uid);
  request["lastMsgId"] = Json::Value::Int64(0);
  request["limit"] = Json::Value::Int(10);
  chat->Send(request.toStyledString(), ID_PULL_MESSAGE_LIST_REQ);
  return true;
}

bool Chat::pullSessionMessageList(long uid) {
  Json::Value request;
  request["from"] = Json::Value::Int64(uid);
  request["to"] = Json::Value::Int64(user->uid);
  request["lastMsgId"] = Json::Value::Int64(0);
  request["limit"] = Json::Value::Int(10);
  chat->Send(request.toStyledString(), ID_PULL_SESSION_MESSAGE_LIST_REQ);

  return true;
}

void Chat::pullFriendListHandle(const Json::Value &response) {
  try {
    Json::Value tmp = response["friendList"];
    for (auto &item : tmp) {
      db::UserInfo::Ptr info(new db::UserInfo());
      info->uid = item["uid"].asInt64();
      info->name = item["name"].asString();
      info->age = item["age"].asInt();
      info->sex = item["sex"].asString();
      info->headImageURL = item["headImageURL"].asString();
      friendMap[info->uid] = info;
    }
  } catch (const std::exception &e) {
    LOG_ERROR(businessLogger, "异常：{}", e.what());
  }
}

void Chat::pullFriendApplyListHandle(const Json::Value &response) {
  Json::Value tmp = response["applyList"];
  for (auto &item : tmp) {
    db::FriendApply::Ptr apply(new db::FriendApply());
    apply->from = item["from"].asInt64();
    apply->to = item["to"].asInt64();
    apply->status = item["status"].asInt();
    apply->content = item["content"].asString();
    apply->createTime = item["createTime"].asString();
    friendApplyMap[apply->to] = apply;
  }
}

void Chat::pullMessageListHandle(const Json::Value &response) {
  Json::Value tmp = response["messageList"];
  long uid = response["uid"].asInt64();
  db::Message::MessageGroup messageList(new std::vector<db::Message::Ptr>());
  for (auto &item : tmp) {
    db::Message::Ptr message(new db::Message());
    message->messageId = item["messageId"].asInt64();
    message->type = item["type"].asInt();
    message->content = item["content"].asString();
    message->status = item["status"].asInt();
    message->sendDateTime = item["sendDateTime"].asString();
    message->readDateTime = item["readDateTime"].asString();
  }
  messageListMap[uid] = messageList;
}

bool Chat::notifyAddFriend(long uid, const std::string &requestMessage) {

  Json::Value addFriendReq;
  addFriendReq["from"] = Json::Value::Int64(user->uid);
  addFriendReq["to"] = Json::Value::Int64(uid);
  addFriendReq["requestMessage"] = requestMessage;
  chat->Send(addFriendReq.toStyledString(), ID_NOTIFY_ADD_FRIEND_REQ);

  friendApplyMap[uid] = std::make_shared<db::FriendApply>();
  friendApplyMap[uid]->from = user->uid;
  friendApplyMap[uid]->to = uid;
  friendApplyMap[uid]->content = requestMessage;
  friendApplyMap[uid]->status = 0;

  return true;
}
bool Chat::replyAddFriend(long uid, bool accept,
                          const std::string &replyMessage) {
  Json::Value addFriendRequest;
  addFriendRequest["from"] = Json::Value::Int64(user->uid);
  addFriendRequest["to"] = Json::Value::Int64(uid);
  addFriendRequest["accept"] = accept;
  addFriendRequest["replyMessage"] = replyMessage;
  LOG_INFO(businessLogger, "request json: {}",
           addFriendRequest.toStyledString());

  chat->Send(addFriendRequest.toStyledString(), ID_REPLY_ADD_FRIEND_REQ);
  friendApplyMap[uid] = std::make_shared<db::FriendApply>();
  friendApplyMap[uid]->status = accept;
  friendApplyMap[uid]->content = replyMessage;

  friendMap[uid] = std::make_shared<db::UserInfo>();
  friendMap[uid]->uid = uid;

  return true;
}

void Chat::onReWrite(long id, const std::string &message, long serviceId,
                     int count) {
  chat->Send(message, serviceId);
  waitAckTimerMap[id] = std::make_shared<net::steady_timer>(
      chat->iocontext, std::chrono::seconds(5));
  waitAckTimerMap[id]->async_wait([this, id, message, serviceId,
                                   count](const boost::system::error_code &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      LOG_INFO(wim::businessLogger, "seq:{} timer canceled", id);
    } else {
      LOG_INFO(wim::businessLogger, "seq:{} timer expired", id);
      if (count + 1 >= 3) {
        LOG_INFO(wim::businessLogger, "重发次数达到最大次数，故障转移中.....");
        arrhythmiaHandle(user->uid);
        return;
      }
      onReWrite(id, message, serviceId, count + 1);
    }
  });
}
void Chat::sendTextMessage(long uid, const std::string &message) {
  Json::Value textMsg;

  // 该id仅是客户端的序列号，其作用是在之后接收到服务器ACK能找到并取消重传定时器
  long seq = generateRandomId();
  textMsg["seq"] = Json::Value::Int64(seq);
  textMsg["from"] = Json::Value::Int64(user->uid);
  textMsg["to"] = Json::Value::Int64(uid);
  textMsg["data"] = message;
  textMsg["sessionKey"] = 0;
  LOG_INFO(businessLogger, "request json: {}", textMsg.toStyledString());

  onReWrite(seq, textMsg.toStyledString(), ID_TEXT_SEND_REQ);
}

// 群聊
bool Chat::createGroup(const std::string &groupName) {
  Json::Value request;
  request["groupName"] = groupName;
  request["uid"] = Json::Value::Int64(user->uid);
  chat->Send(request.toStyledString(), ID_GROUP_CREATE_REQ);
  return true;
}
bool Chat::joinGroup(long groupId, const std::string &requestMessage) {
  Json::Value request;
  request["gid"] = Json::Value::Int64(groupId);
  request["uid"] = Json::Value::Int64(user->uid);
  request["requestMessage"] = requestMessage;
  chat->Send(request.toStyledString(), ID_GROUP_NOTIFY_JOIN_REQ);
  return true;
}
bool Chat::replyJoinGroup(long groupId, long requestorUid, bool accept) {
  Json::Value request;
  request["gid"] = Json::Value::Int64(groupId);
  request["managerUid"] = Json::Value::Int64(user->uid);
  request["requestorUid"] = Json::Value::Int64(requestorUid);
  request["accept"] = accept;
  chat->Send(request.toStyledString(), ID_GROUP_REPLY_JOIN_REQ);
  return true;
}
bool Chat::quitGroup(long groupId) {
  Json::Value request;
  request["groupId"] = Json::Value::Int64(groupId);
  request["uid"] = Json::Value::Int64(user->uid);
  chat->Send(request.toStyledString(), ID_GROUP_QUIT_REQ);
  return true;
}
bool Chat::sendGroupMessage(long groupId, const std::string &message) {
  Json::Value request;
  request["groupId"] = Json::Value::Int64(groupId);
  request["uid"] = Json::Value::Int64(user->uid);
  request["text"] = message;
  chat->Send(request.toStyledString(), ID_GROUP_TEXT_SEND_REQ);
  return true;
}
bool Chat::pullGroupMember(long groupId) {
  Json::Value request;
  request["groupId"] = Json::Value::Int64(groupId);
  request["uid"] = Json::Value::Int64(user->uid);
  chat->Send(request.toStyledString(), ID_GROUP_PULL_MEMBER_REQ);
  return true;
}

bool Chat::pullGroupMessage(long groupId, long lastMsgId, int limit) {
  return true;
}

void Chat::createGroupHandle(Json::Value &response) {
  db::GroupManager info;

  info.gid = response["groupId"].asInt64();
  info.sessionKey = response["sessionKey"].asInt64();
  info.createTime = response["createTime"].asString();
}

void Chat::joinGroupSenderHandle(Json::Value &response) {}
void Chat::joinGroupRecvierHandle(Json::Value &response) {
  try {
    long seq = response["seq"].asInt64();

    bool missCache = CheckAckCache(seq);
    if (missCache)
      return;

    Json::Value ack = response["seq"];
    ack["uid"] = Json::Value::Int64(user->uid);
    chat->Send(ack.toStyledString(), ID_ACK);
  } catch (std::exception &e) {
    LOG_ERROR(businessLogger, "异常：{}", e.what());
  }
}
void Chat::replyJoinGroupSenderHandle(Json::Value &response) {}
void Chat::replyJoinGroupRecvierHandle(Json::Value &response) {

  long seq = response["seq"].asInt64();
  bool missCache = CheckAckCache(seq);
  if (missCache) {
    return;
  }
  Json::Value ack = response["seq"];
  ack["uid"] = Json::Value::Int64(user->uid);
  chat->Send(ack.toStyledString(), ID_ACK);
}

void Chat::quitGroupHandle(Json::Value &response) {}
void Chat::sendGroupTextMessageHandle(Json::Value &response) {}
void Chat::pullGroupMemberHandle(Json::Value &response) {

  Json::Value list = response["memberList"];
  long gid = response["groupId"].asInt64();
  groupMemberMap[gid] = {};

  for (auto &item : list) {
    db::GroupMember::Ptr member;
    member->uid = item["uid"].asInt64();
    member->memberName = item["name"].asString();
    member->joinTime = item["joinTime"].asString();
    member->role = item["role"].asInt();
    member->speech = item["speech"].asInt();
    groupMemberMap[gid].push_back(member);
  }
}
void Chat::pullGroupMessageHandle(Json::Value &response) {}

// 文件
bool Chat::uploadFile(const std::string &fileName) {
  Json::Value request;

  std::fstream file(Configer::getSaveFilePath() + fileName,
                    std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", fileName);
    return false;
  }
  static const int chunkSize = 1024 * 1024; // 1MB
  char buffer[chunkSize];

  long seq = generateRandomId();
  db::File::Ptr fileInfo(new db::File());
  fileInfo->seq = seq;
  fileInfo->type = db::File::Type::TEXT;
  fileInfo->name = fileName;

  fileMap[fileName] = fileInfo;

  while (!file.eof()) {
    file.read(buffer, chunkSize);
    fileInfo->offset += chunkSize;
    fileInfo->data.append(buffer, file.gcount());

    request["seq"] = Json::Value::Int64(seq);
    request["uid"] = Json::Value::Int64(user->uid);
    request["data"] = buffer;
    request["fileName"] = fileName;
    request["type"] = "TEXT";

    // onReWrite(seq, request.toStyledString(), ID_FILE_UPLOAD_REQ);

    chat->Send(request.toStyledString(), ID_FILE_UPLOAD_REQ);
    seq++;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  file.close();
  return true;
}
bool Chat::sendFile(long toId, const std::string &filePath) {
  Json::Value request;

  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", filePath);
    return false;
  }
  static const int chunkSize = 1024 * 1024; // 1MB
  char buffer[chunkSize];

  db::File::Ptr fileInfo(new db::File());
  fileInfo->seq = generateRandomId();
  fileInfo->type = db::File::Type::TEXT; // 待调整
  fileInfo->name = filePath;

  fileMap[filePath] = fileInfo;

  while (!file.eof()) {
    file.read(buffer, chunkSize);
    fileInfo->offset += chunkSize;
    fileInfo->data.append(buffer, file.gcount());

    long chunkSeq = generateRandomId();
    request["seq"] = Json::Value::Int64(chunkSeq);
    request["from"] = Json::Value::Int64(user->uid);
    request["to"] = Json::Value::Int64(toId);
    request["data"] = buffer;
    request["sessionKey"] = 0;
    request["type"] = 2;
    onReWrite(chunkSeq, request.toStyledString(), ID_FILE_SEND_REQ);
    // 客户端主动P99延迟，保顺序
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  file.close();
  return true;
}
void Chat::sendFileHandle(Json::Value &response) {
  std::string name = response["name"].asString();
  long seq = response["seq"].asInt64();
  long toUid = response["to"].asInt64();
  int status = response["status"].asInt();

  LOG_INFO(businessLogger, "recv file: {}, seq: {}, toUid: {}, status: {}",
           name, seq, toUid, status);
}

void Chat::recvFileHandle(Json::Value &response) {
  std::string name = response["name"].asString();
  long seq = response["seq"].asInt64();
  long fromUid = response["from"].asInt64();
  std::string data = response["data"].asString();

  std::string savePath =
      "./" + std::to_string(user->uid) + "/files/" + name + ".txt";
  std::fstream file(savePath, std::ios::out | std::ios::binary | std::ios::app);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", name);
  }
  file.write(data.c_str(), data.size());
  file.close();

  // ack todo...
}
}; // namespace wim