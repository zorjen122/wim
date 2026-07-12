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

namespace wim {

static std::shared_ptr<net::steady_timer> waitPongTimer;
static std::map<long, std::shared_ptr<net::steady_timer>> waitAckTimerMap;

static long generateRandomId() {
  static std::random_device rd;   // 使用硬件随机数生成器
  static std::mt19937 gen(rd());  // Mersenne Twister 作为随机数引擎
  static std::uniform_int_distribution<long> dis(1000000000,
                                                 9999999999);  // 随机数范围

  return dis(gen);  // 返回一个随机数
}

void Chat::nullHandle(const TcpPacket &response) {}

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
      std::bind(&Chat::replyJoinGroupSenderHandle, this, _1);
  handleMap[ID_GROUP_REPLY_JOIN_REQ] =
      std::bind(&Chat::replyJoinGroupRecvierHandle, this, _1);

  handleMap[ID_GROUP_TEXT_SEND_RSP] =
      std::bind(&Chat::textSenderHandle, this, _1);
  handleMap[ID_GROUP_TEXT_SEND_REQ] =
      std::bind(&Chat::textRecvierHandle, this, _1);

  handleMap[ID_GROUP_QUIT_RSP] = std::bind(&Chat::quitGroupHandle, this, _1);
  handleMap[ID_GROUP_PULL_MEMBER_RSP] =
      std::bind(&Chat::pullGroupMemberHandle, this, _1);

  handleMap[ID_FILE_SEND_RSP] = std::bind(&Chat::sendFileHandle, this, _1);
  handleMap[ID_FILE_SEND_REQ] = std::bind(&Chat::recvFileHandle, this, _1);

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

void Chat::replyAddFriendSenderHandle(const TcpPacket &response) {
  auto fromUid = response.uid();
  auto accept = response.accept();
  auto replyMessage = response.reply_message();
  if (friendApplyMap.find(fromUid) == friendApplyMap.end()) {
    spdlog::error("好友申请回应信息未保存到本地, uid:{}", fromUid);
    return;
  }
  if (TcpPacketError(response) == ErrorCodes::Success) {
    friendApplyMap[fromUid]->status = accept;
    friendApplyMap[fromUid]->content = replyMessage;
  }
}

void Chat::replyAddFriendRecvierHandle(const TcpPacket &response) {
  long uid = response.has_uid() ? response.uid() : response.from();

  if (response.has_seq()) {
    long seq = response.seq();
    bool missCache = CheckAckCache(seq);
    if (missCache)
      return;
    TcpPacket ack;
    ack.set_seq(response.seq());
    ack.set_uid(user->uid);
    chat->Send(SerializeTcpPacket(ack), ID_ACK);
  }

  // 服务端处理失败时则回复原来状态
  if (response.has_error() && response.error() != ErrorCodes::Success) {
    LOG_WARN(businessLogger, "答复好友申请出现了异常, message: {}",
             response.message());
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

void Chat::notifyAddFriendSenderHandle(const TcpPacket &response) {
  long uid = response.uid();

  // 服务端处理失败时则回复原来状态
  if (TcpPacketError(response) != ErrorCodes::Success) {
    LOG_WARN(businessLogger, "申请好友失败, message: {}", response.message());
    if (friendApplyMap.find(uid) == friendApplyMap.end()) {
      LOG_INFO(businessLogger, "申请好友信息未保存到本地, uid:{}", uid);
      return;
    }
    friendApplyMap.erase(uid);
    return;
  }
}
void Chat::notifyAddFriendRecvierHandle(const TcpPacket &response) {
  db::FriendApply::Ptr apply(new db::FriendApply());
  apply->from = response.from();
  apply->content =
      response.has_content() ? response.content() : response.request_message();
  apply->to = user->uid;
  apply->status = 0;
  friendApplyMap[apply->from] = apply;
}
void Chat::loginInitHandle(const TcpPacket &response) {
  auto errcode = TcpPacketError(response);
  {
    std::lock_guard<std::mutex> lock(loginMutex);
    loginInitDone = true;
    loginInitOk = errcode != -1;
  }
  loginCv.notify_all();

  if (errcode == -1) {
    LOG_INFO(businessLogger, "登录失败，请重试");
    return;
  }

  std::string name = response.name();
  short age = response.age();
  std::string sex = response.sex();
  std::string headImageURL = response.head_image_url();
}

bool Chat::waitLoginReady(int timeoutSeconds) {
  std::unique_lock<std::mutex> lock(loginMutex);
  bool hasResponse =
      loginCv.wait_for(lock, std::chrono::seconds(timeoutSeconds),
                       [this] { return loginInitDone; });
  return hasResponse && loginInitOk;
}

void Chat::textSenderHandle(const TcpPacket &response) {
  if (!response.has_seq()) {
    LOG_INFO(businessLogger, "发送响应未携带 seq，响应: {}",
             TcpPacketDebugString(response));
    return;
  }
  long seq = response.seq();
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
  TcpPacket ack;
  ack.set_seq(seq);
  ack.set_uid(user->uid);
  bool missCache = seqCacheExpireMap.find(seq) != seqCacheExpireMap.end();
  // 若ACK已被服务端收到，则意味着其不会重发，反之则重发，若其重发，则复发一次ACK给客户端
  if (missCache) {
    chat->Send(SerializeTcpPacket(ack), ID_ACK);
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

void Chat::textRecvierHandle(const TcpPacket &response) {
  long from = response.from();
  long to = response.to();
  long seq = response.seq();
  std::string data = response.data();
  int type = response.type();

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

  TcpPacket ack;
  ack.set_seq(response.seq());
  ack.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(ack), ID_ACK);
}
void Chat::uploadFileHandle(const TcpPacket &response) {}

void Chat::handleRun(Tlv::Ptr protocolData) {
  TcpPacket response;
  if (!ParseTcpPacket(protocolData->getDataString(), response)) {
    LOG_WARN(businessLogger,
             "响应 Protobuf 解析失败, 服务: {}, 原始数据大小: {}",
             getServiceIdString(protocolData->id), protocolData->getDataSize());
    return;
  }

  auto errcode = TcpPacketError(response);
  if (errcode == -1) {
    LOG_WARN(businessLogger, "响应异常： {}", TcpPacketDebugString(response));
  }

  auto handleFunc = handleMap.find(protocolData->id);

  if (handleFunc == handleMap.end()) {
    LOG_WARN(businessLogger, "没有这样的服务响应函数, 响应码: {}, response: {}",
             protocolData->id, TcpPacketDebugString(response));
    return;
  }

  LOG_DEBUG(wim::businessLogger, "响应服务: {}, 响应包: {}",
            getServiceIdString(protocolData->id),
            TcpPacketDebugString(response));
  try {
    handleFunc->second(response);
  } catch (const std::exception &e) {
    LOG_ERROR(businessLogger, "处理响应异常, 服务: {}, 异常: {}, 响应: {}",
              getServiceIdString(protocolData->id), e.what(),
              TcpPacketDebugString(response));
  }
}

void Chat::initUserInfo(db::UserInfo::Ptr userInfo) {
  TcpPacket request;
  request.set_name(userInfo->name);
  request.set_age(userInfo->age);
  request.set_sex(userInfo->sex);
  chat->Send(SerializeTcpPacket(request), ID_INIT_USER_INFO_REQ);
}

void Chat::initUserInfoHandle(const TcpPacket &response) {
  // pull...
}
bool Chat::login(bool isFirstLogin) {
  TcpPacket loginRequest;
  loginRequest.set_uid(user->uid);

  chat->Send(SerializeTcpPacket(loginRequest), ID_LOGIN_INIT_REQ);
  pullFriendList();
  pullFriendApplyList();
  pullMessageList();
  return true;
}

void Chat::quit() {
  TcpPacket quitRequest;
  quitRequest.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(quitRequest), ID_USER_QUIT_REQ);
}

bool Chat::ping() {
  TcpPacket ping;
  ping.set_uid(user->uid);

  std::string pingBuffer = SerializeTcpPacket(ping);
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
void Chat::pingHandle(const TcpPacket &response) {
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

void Chat::serachUserHandle(const TcpPacket &response) {
  long uid = response.uid();
  std::string name = response.name();
  short age = response.age();
  std::string sex = response.sex();
  std::string headImageURL = response.head_image_url();
}

bool Chat::searchUser(const std::string &username) {
  TcpPacket searchReq;
  searchReq.set_username(username);
  chat->Send(SerializeTcpPacket(searchReq), ID_SEARCH_USER_REQ);

  LOG_INFO(wim::businessLogger, "search user: {}", username);
  return true;
}

bool Chat::pullFriendList() {
  TcpPacket request;
  request.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(request), ID_PULL_FRIEND_LIST_REQ);
  return true;
}
bool Chat::pullFriendApplyList() {
  TcpPacket request;
  request.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(request), ID_PULL_FRIEND_APPLY_LIST_REQ);
  return true;
}

bool Chat::pullMessageList() {
  TcpPacket request;
  request.set_uid(user->uid);
  request.set_last_msg_id(0);
  request.set_limit(10);
  chat->Send(SerializeTcpPacket(request), ID_PULL_MESSAGE_LIST_REQ);
  return true;
}

bool Chat::pullSessionMessageList(long uid) {
  TcpPacket request;
  request.set_from(uid);
  request.set_to(user->uid);
  request.set_last_msg_id(0);
  request.set_limit(10);
  chat->Send(SerializeTcpPacket(request), ID_PULL_SESSION_MESSAGE_LIST_REQ);

  return true;
}

void Chat::pullFriendListHandle(const TcpPacket &response) {
  try {
    for (const auto &item : response.friend_list()) {
      db::UserInfo::Ptr info(new db::UserInfo());
      info->uid = item.uid();
      info->name = item.name();
      info->age = item.age();
      info->sex = item.sex();
      info->headImageURL = item.head_image_url();
      friendMap[info->uid] = info;
    }
  } catch (const std::exception &e) {
    LOG_ERROR(businessLogger, "异常：{}", e.what());
  }
}

void Chat::pullFriendApplyListHandle(const TcpPacket &response) {
  for (const auto &item : response.apply_list()) {
    db::FriendApply::Ptr apply(new db::FriendApply());
    apply->from = item.from();
    apply->to = item.to();
    apply->status = item.status();
    apply->content = item.content();
    apply->createTime = item.create_time().empty() ? item.apply_date_time()
                                                   : item.create_time();
    friendApplyMap[apply->to] = apply;
  }
}

void Chat::pullMessageListHandle(const TcpPacket &response) {
  long uid = response.uid();
  db::Message::MessageGroup messageList(new std::vector<db::Message::Ptr>());
  for (const auto &item : response.message_list()) {
    db::Message::Ptr message(new db::Message());
    message->messageId = item.message_id();
    message->type = item.type();
    message->content = item.content();
    message->status = item.status();
    message->sendDateTime = item.send_date_time();
    message->readDateTime = item.read_date_time();
    messageList->push_back(message);
  }
  messageListMap[uid] = messageList;
}

bool Chat::notifyAddFriend(long uid, const std::string &requestMessage) {
  TcpPacket addFriendReq;
  addFriendReq.set_from(user->uid);
  addFriendReq.set_to(uid);
  addFriendReq.set_request_message(requestMessage);
  chat->Send(SerializeTcpPacket(addFriendReq), ID_NOTIFY_ADD_FRIEND_REQ);

  friendApplyMap[uid] = std::make_shared<db::FriendApply>();
  friendApplyMap[uid]->from = user->uid;
  friendApplyMap[uid]->to = uid;
  friendApplyMap[uid]->content = requestMessage;
  friendApplyMap[uid]->status = 0;

  return true;
}
bool Chat::replyAddFriend(long uid, bool accept,
                          const std::string &replyMessage) {
  TcpPacket addFriendRequest;
  addFriendRequest.set_from(user->uid);
  addFriendRequest.set_to(uid);
  addFriendRequest.set_accept(accept);
  addFriendRequest.set_reply_message(replyMessage);
  LOG_INFO(businessLogger, "request packet: {}",
           TcpPacketDebugString(addFriendRequest));

  chat->Send(SerializeTcpPacket(addFriendRequest), ID_REPLY_ADD_FRIEND_REQ);
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
  TcpPacket textMsg;

  // 该id仅是客户端的序列号，其作用是在之后接收到服务器ACK能找到并取消重传定时器
  long seq = generateRandomId();
  textMsg.set_seq(seq);
  textMsg.set_from(user->uid);
  textMsg.set_to(uid);
  textMsg.set_data(message);
  textMsg.set_session_key(0);
  LOG_INFO(businessLogger, "request packet: {}", TcpPacketDebugString(textMsg));

  onReWrite(seq, SerializeTcpPacket(textMsg), ID_TEXT_SEND_REQ);
}

// 群聊
bool Chat::createGroup(const std::string &groupName) {
  TcpPacket request;
  request.set_group_name(groupName);
  request.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(request), ID_GROUP_CREATE_REQ);
  return true;
}
bool Chat::joinGroup(long groupId, const std::string &requestMessage) {
  TcpPacket request;
  request.set_gid(groupId);
  request.set_uid(user->uid);
  request.set_request_message(requestMessage);
  chat->Send(SerializeTcpPacket(request), ID_GROUP_NOTIFY_JOIN_REQ);
  return true;
}
bool Chat::replyJoinGroup(long groupId, long requestorUid, bool accept) {
  TcpPacket request;
  request.set_gid(groupId);
  request.set_manager_uid(user->uid);
  request.set_requestor_uid(requestorUid);
  request.set_accept(accept);
  chat->Send(SerializeTcpPacket(request), ID_GROUP_REPLY_JOIN_REQ);
  return true;
}
bool Chat::quitGroup(long groupId) {
  TcpPacket request;
  request.set_group_id(groupId);
  request.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(request), ID_GROUP_QUIT_REQ);
  return true;
}
bool Chat::sendGroupMessage(long groupId, const std::string &message) {
  TcpPacket request;
  request.set_group_id(groupId);
  request.set_uid(user->uid);
  request.set_data(message);
  chat->Send(SerializeTcpPacket(request), ID_GROUP_TEXT_SEND_REQ);
  return true;
}
bool Chat::pullGroupMember(long groupId) {
  TcpPacket request;
  request.set_group_id(groupId);
  request.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(request), ID_GROUP_PULL_MEMBER_REQ);
  return true;
}

bool Chat::pullGroupMessage(long groupId, long lastMsgId, int limit) {
  return true;
}

void Chat::createGroupHandle(TcpPacket &response) {
  db::GroupManager info;

  info.gid = response.gid();
  info.sessionKey = response.session_key();
  info.createTime = response.create_time();
}

void Chat::joinGroupSenderHandle(TcpPacket &response) {}
void Chat::joinGroupRecvierHandle(TcpPacket &response) {
  try {
    long seq = response.seq();

    bool missCache = CheckAckCache(seq);
    if (missCache)
      return;

    TcpPacket ack;
    ack.set_seq(response.seq());
    ack.set_uid(user->uid);
    chat->Send(SerializeTcpPacket(ack), ID_ACK);
  } catch (std::exception &e) {
    LOG_ERROR(businessLogger, "异常：{}", e.what());
  }
}
void Chat::replyJoinGroupSenderHandle(TcpPacket &response) {}
void Chat::replyJoinGroupRecvierHandle(TcpPacket &response) {
  long seq = response.seq();
  bool missCache = CheckAckCache(seq);
  if (missCache) {
    return;
  }
  TcpPacket ack;
  ack.set_seq(response.seq());
  ack.set_uid(user->uid);
  chat->Send(SerializeTcpPacket(ack), ID_ACK);
}

void Chat::quitGroupHandle(TcpPacket &response) {}
void Chat::sendGroupTextMessageHandle(TcpPacket &response) {}
void Chat::pullGroupMemberHandle(TcpPacket &response) {
  long gid = response.group_id();
  groupMemberMap[gid] = {};

  for (const auto &item : response.member_list()) {
    db::GroupMember::Ptr member;
    member = std::make_shared<db::GroupMember>();
    member->uid = item.uid();
    member->memberName = item.name();
    member->joinTime = item.join_time();
    member->role = item.role();
    member->speech = item.speech();
    groupMemberMap[gid].push_back(member);
  }
}
void Chat::pullGroupMessageHandle(TcpPacket &response) {}

// 文件
bool Chat::uploadFile(const std::string &fileName) {
  std::fstream file(Configer::getSaveFilePath() + fileName,
                    std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", fileName);
    return false;
  }
  static const int chunkSize = 1024 * 1024;  // 1MB
  char buffer[chunkSize];

  long seq = generateRandomId();
  db::File::Ptr fileInfo(new db::File());
  fileInfo->seq = seq;
  fileInfo->type = db::File::Type::TEXT;
  fileInfo->name = fileName;

  fileMap[fileName] = fileInfo;

  while (!file.eof()) {
    file.read(buffer, chunkSize);
    auto bytesRead = file.gcount();
    if (bytesRead <= 0)
      break;
    fileInfo->offset += bytesRead;
    fileInfo->data.append(buffer, bytesRead);

    TcpPacket request;
    request.set_seq(seq);
    request.set_uid(user->uid);
    request.set_data(std::string(buffer, bytesRead));
    request.set_file_name(fileName);
    request.set_file_type("TEXT");

    chat->Send(SerializeTcpPacket(request), ID_FILE_UPLOAD_REQ);
    seq++;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  file.close();
  return true;
}
bool Chat::sendFile(long toId, const std::string &filePath) {
  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", filePath);
    return false;
  }
  static const int chunkSize = 1024 * 1024;  // 1MB
  char buffer[chunkSize];

  db::File::Ptr fileInfo(new db::File());
  fileInfo->seq = generateRandomId();
  fileInfo->type = db::File::Type::TEXT;  // 待调整
  fileInfo->name = filePath;

  fileMap[filePath] = fileInfo;

  while (!file.eof()) {
    file.read(buffer, chunkSize);
    auto bytesRead = file.gcount();
    if (bytesRead <= 0)
      break;
    fileInfo->offset += bytesRead;
    fileInfo->data.append(buffer, bytesRead);

    long chunkSeq = generateRandomId();
    TcpPacket request;
    request.set_seq(chunkSeq);
    request.set_from(user->uid);
    request.set_to(toId);
    request.set_data(std::string(buffer, bytesRead));
    request.set_session_key(0);
    request.set_type(2);
    onReWrite(chunkSeq, SerializeTcpPacket(request), ID_FILE_SEND_REQ);
    // 客户端主动P99延迟，保顺序
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  file.close();
  return true;
}
void Chat::sendFileHandle(TcpPacket &response) {
  std::string name = response.name();
  long seq = response.seq();
  long toUid = response.to();
  std::string status = response.status();

  LOG_INFO(businessLogger, "recv file: {}, seq: {}, toUid: {}, status: {}",
           name, seq, toUid, status);
}

void Chat::recvFileHandle(TcpPacket &response) {
  std::string name = response.name();
  long seq = response.seq();
  long fromUid = response.from();
  std::string data = response.data();

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
};  // namespace wim
