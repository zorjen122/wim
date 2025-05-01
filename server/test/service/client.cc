#include "client.h"
#include "Const.h"
#include "DbGlobal.h"
#include "Logger.h"
#include "global.h"
#include "service/chatSession.h"
#include <boost/asio/steady_timer.hpp>
#include <cstddef>
#include <iostream>
#include <jsoncpp/json/value.h>

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cassert>
#include <chrono>
#include <fstream>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>

namespace wim {

static std::shared_ptr<net::steady_timer> waitPongTimer;
static std::map<long, std::shared_ptr<net::steady_timer>> waitAckTimerMap;

long generateSequeueId() {
  std::atomic<long> seq{0};
  return seq.fetch_add(1);
}

Gate::Gate(net::io_context &iocontext, const std::string &host,
           const std::string &port)
    : context(iocontext), stream(iocontext) {
  if (host.empty() || port.empty())
    throw std::invalid_argument("host or port is empty");

  Defer _([this]() { __clearStatusMessage(); });

  tcp::resolver resolver(context);

  endpoint = resolver.resolve(host, port);

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::get);
  request.target(__GateTestPath__);
  request.version(11);
  request.set(http::field::host, host);
  request.set(http::field::content_type, "application/json");

  http::write(stream, request, ec);
  if (ec.failed())
    throw std::runtime_error("write failed: " + ec.message());

  http::read(stream, buffer, response, ec);
  if (ec.failed())
    throw std::runtime_error("read failed: " + ec.message());

  auto bodyBuffer = response.body().data();
  auto stringBody = beast::buffers_to_string(bodyBuffer);

  LOG_INFO(wim::businessLogger, "response message: {}", stringBody);
}

std::pair<Endpoint, int> Gate::signIn(const std::string &username,
                                      const std::string &password) {
  LOG_INFO(wim::businessLogger, "sign in as {}, password as {}", username,
           password);

  Defer _([this]() { __clearStatusMessage(); });

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::post);
  request.target(__GateSigninPath__);

  Json::Value requestData;
  requestData["username"] = username;
  requestData["password"] = password;

  request.body() = requestData.toStyledString();
  request.prepare_payload();
  http::write(stream, request, ec);
  if (ec.failed())
    throw std::runtime_error("write failed: " + ec.message());
  http::read(stream, buffer, response, ec);
  if (ec.failed())
    throw std::runtime_error("read failed: " + ec.message());

  auto stringBody = __parseResponse();
  LOG_INFO(wim::businessLogger, "response status: {}", stringBody);

  Json::Reader reader;
  Json::Value responseData;
  reader.parse(stringBody, responseData);

  int init = responseData["init"].asInt();

  Endpoint chatEndpoint(responseData["ip"].asString(),
                        responseData["port"].asString());

  auto uid = responseData["uid"].asInt64();
  users[username] =
      std::make_shared<db::User>(0, uid, username, password, "null");

  return {chatEndpoint, init};
}

bool Gate::signUp(const std::string &username, const std::string &password,
                  const std::string &email) {
  LOG_INFO(wim::businessLogger, "sign in as {}, password as {}", username,
           password);

  Defer _([this]() { __clearStatusMessage(); });

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::post);
  request.target(__GateSignupPath__);

  Json::Value requestData;
  requestData["username"] = username;
  requestData["password"] = password;
  requestData["email"] = email;

  request.body() = requestData.toStyledString();
  request.prepare_payload();

  LOG_INFO(wim::businessLogger, "http-write({}): request body: {}",
           request.target(), (request.body().data()));
  http::write(stream, request, ec);
  if (ec.failed())
    throw std::runtime_error("write failed: " + ec.message());
  http::read(stream, buffer, response, ec);
  if (ec.failed())
    throw std::runtime_error("read failed: " + ec.message());

  auto stringBody = __parseResponse();
  LOG_INFO(wim::businessLogger, "response message: {} | status:{}", stringBody,
           response.result_int());

  Json::Value responseData = __parseJson(stringBody);
  auto uid = responseData["uid"].asInt64();
  users[username] =
      std::make_shared<db::User>(0, uid, username, password, email);

  return true;
}
bool Gate::signOut() { return true; }
bool Gate::fogetPassword(const std::string &username) { return true; }

std::string Gate::__parseResponse() {
  auto bodyBuffer = response.body().data();
  return beast::buffers_to_string(bodyBuffer);
}
Json::Value __parseJson(const std::string &source) {
  Json::Reader reader;
  Json::Value responseData;
  bool parseSuccess = reader.parse(source, responseData);
  if (parseSuccess == false)
    throw std::runtime_error("parse json failed");
  return responseData;
}
void Gate::__clearStatusMessage() {
  request.body().clear();
  response.body().clear();
  buffer.clear();
}

void Text_recv_rsp_handler(int from, int to, int seq, std::string text) {
  spdlog::info("[{}]: {}\t|seq:{}", from, text, seq);
}

Chat::Chat() {}
bool Chat::login(bool isFirstLogin) {

  Json::Value loginRequest;
  loginRequest["uid"] = Json::Value::Int64(user->uid);

  if (isFirstLogin) {
    loginRequest["init"] = true;
    loginRequest["uid"] = Json::Value::Int64(userInfo->uid);
    loginRequest["name"] = userInfo->name;
    loginRequest["age"] = userInfo->age;
    loginRequest["sex"] = userInfo->sex;
    loginRequest["headImageURL"] = userInfo->headImageURL;
  }
  chat->Send(loginRequest.toStyledString(), ID_LOGIN_INIT_REQ);

  return true;
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
      // spdlog::info("onRePongWrite timer canceled | uid-{}", user->uid);
      return;
    } else {
      // 暂行方案，可进一步考虑租约机制，未超时采用n秒租约以免于频繁Ping，若超时一次后则采用默认心跳
      static const int max_retry_count = 3;
      if (count + 1 > max_retry_count) {
        arrhythmiaHandle(user->uid);
        return;
      }

      ping();
      LOG_INFO(businessLogger, "ping timeout, retry count: {}", count);
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
  spdlog::info("arrhythmiaHandle: uid-{}", uid);
}

void Chat::handleRun(Tlv::Ptr protocolData) {

  Json::Value response;
  Json::Reader reader;
  reader.parse(protocolData->getDataString(), response);
  auto errcode = response["error"].asInt();
  if (errcode == -1) {
    spdlog::warn("handleRun: error is {}", errcode);
  }
  LOG_INFO(businessLogger, "response ID: {}", protocolData->id);
  switch (protocolData->id) {
  case ID_PING_RSP: {
    pingHandle(response);
    break;
  }
  case ID_SEARCH_USER_RSP: {
    serachUserHandle(response);
    break;
  }
  case ID_PULL_FRIEND_LIST_RSP: {
    pullFriendListHandle(response);
    break;
  }
  case ID_PULL_FRIEND_APPLY_LIST_RSP: {
    pullFriendApplyListHandle(response);
    break;
  }
  case ID_PULL_MESSAGE_LIST_RSP: {
    pullMessageListHandle(response);
    break;
  }
  // 通知请求者添加的好友请求被响应
  case ID_REPLY_ADD_FRIEND_REQ: {
    auto fromUid = response["fromUid"].asInt64();
    auto accept = response["accept"].asBool();
    auto replyMessage = response["content"].asString();

    friendApplyMap[fromUid]->status = accept;
    friendApplyMap[fromUid]->content = replyMessage;

    spdlog::info("reply add friend response, uid:{}, accept:{}, message:{}",
                 fromUid, accept, replyMessage);
    break;
  }
  case ID_REPLY_ADD_FRIEND_RSP: {
    spdlog::info("reply add friend success, recvRsp {}",
                 response.toStyledString());
    break;
  }
  case ID_NOTIFY_ADD_FRIEND_RSP: {
    db::FriendApply::Ptr apply(new db::FriendApply());
    apply->fromUid = user->uid;
    apply->toUid = response["toUid"].asInt64();
    apply->content = response["content"].asString();

    friendApplyMap[apply->toUid] = apply;

    LOG_INFO(wim::businessLogger, "add friend success, response {}",
             response.toStyledString());
    break;
  }
  case ID_LOGIN_INIT_RSP: {
    auto errcode = response["error"].asInt();
    if (errcode == -1) {
      return;
    }

    std::string name = response["name"].asString();
    short age = response["age"].asInt();
    std::string sex = response["sex"].asString();
    std::string headImageURL = response["headImageURL"].asString();

    // LOG_INFO(wim::businessLogger, "OK");
    LOG_INFO(wim::businessLogger,
             "userInfo: name {}, age {}, sex {}, headImageURL {}", name, age,
             sex, headImageURL);
    break;
  }
  case ID_TEXT_SEND_REQ: {
    long from = response["fromUid"].asInt64();
    long to = response["toUid"].asInt64();
    long seq = response["seq"].asInt64();
    std::string text = response["text"].asString();

    Json::Value ack;
    ack["seq"] = Json::Value::Int64(seq);
    ack["fromUid"] = Json::Value::Int64(from);
    ack["toUid"] = Json::Value::Int64(to);

    bool missCache = seqCacheExpireMap.find(seq) != seqCacheExpireMap.end();
    // 若ACK已被服务端收到，则意味着其不会重发，反之则重发，若其重发，则复发一次ACK给客户端
    if (missCache == false) {
      seqCacheExpireMap[seq] = std::make_shared<net::steady_timer>(
          chat->iocontext, std::chrono::seconds(5));
      seqCacheExpireMap[seq]->async_wait(
          [this, seq](const boost::system::error_code &ec) {
            if (ec == boost::asio::error::timed_out) {
              seqCacheExpireMap.erase(seq);
              spdlog::info("seq:{} timer expired", seq);
            } else if (ec == boost::asio::error::operation_aborted) {
              spdlog::info("seq:{} timer canceled", seq);
            }
          });

      // 推入到消息队列中，每隔100ms拉取（确保%99有序————具体保障P99的延迟数值待分析)

      Message message;
      message.id = seq;
      message.fromUid = from;
      message.toUid = to;
      message.type = Message::Type::TEXT;
      message.source = text;
      messageQueue.push_back(message);

      // 查看定时器是否已启动，消费消息的触发逻辑放在handleRun中，则是惰性的，以避免重复启动定时器
      bool onRunMessageTimer = messageReadTimer->expiry() >
                               boost::asio::steady_timer::clock_type::now();
      if (onRunMessageTimer) {
        messageReadTimer->async_wait(
            [this](const boost::system::error_code &ec) {
              if (ec == boost::asio::error::timed_out) {
                std::lock_guard<std::mutex> lock(comsumeMessageMutex);
                std::sort(messageQueue.begin(), messageQueue.end(),
                          [](const Message &a, const Message &b) {
                            return a.id < b.id;
                          });
                for (auto &message : messageQueue) {
                  std::cout << "view message Id: " << message.id << "\n";
                  Text_recv_rsp_handler(message.fromUid, message.toUid,
                                        message.id, message.source);
                }
                messageQueue.clear();
              } else if (ec == boost::asio::error::operation_aborted) {
                spdlog::info("messageReadTimer canceled");
              }
            });
      }
      chat->Send(ack.toStyledString(), ID_ACK);
    } else {
      chat->Send(ack.toStyledString(), ID_ACK);
    }
    break;
  }
  case ID_TEXT_SEND_RSP: {
    long seq = response["seq"].asInt64();
    auto timer = waitAckTimerMap[seq];
    if (timer == nullptr) {
      LOG_WARN(businessLogger, "seq: {} -> timer not found", seq);
      return;
    }
    spdlog::info("ACK: {}", response.toStyledString());
    timer->cancel();
    break;
  }
  case ID_NOTIFY_ADD_FRIEND_REQ: {
    spdlog::info("\nhandleRun: ID_NOTIFY_ADD_FRIEND_REQ, data: {}\n",
                 response.toStyledString());
    db::FriendApply::Ptr apply(new db::FriendApply());
    apply->fromUid = response["fromUid"].asInt64();
    apply->content = response["content"].asString();
    apply->toUid = user->uid;
    apply->status = 0;
    friendApplyMap[apply->fromUid] = apply;
    break;
  }
  default:
    spdlog::error("没有这样的服务响应ID");
  }
}

void Chat::serachUserHandle(const Json::Value &response) {

  long uid = response["uid"].asInt64();
  std::string name = response["name"].asString();
  short age = response["age"].asInt();
  short sex = response["sex"].asInt();
  std::string headImageURL = response["headImageURL"].asString();

  LOG_INFO(wim::businessLogger,
           "search user: uid {}, name {}, age {}, sex {}, headImageURL {}", uid,
           name, age, sex, headImageURL);
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
bool Chat::pullMessageList(long uid) {
  Json::Value request;
  request["fromUid"] = Json::Value::Int64(user->id);
  request["toId"] = Json::Value::Int64(uid);
  request["lasgMsgId"] = Json::Value::Int64(0);
  request["limit"] = Json::Value::Int(10);
  chat->Send(request.toStyledString(), ID_PULL_MESSAGE_LIST_REQ);

  return true;
}

bool Chat::pullFriendListHandle(const Json::Value &response) {
  Json::Value tmp = response["friendList"];
  for (auto &item : tmp) {
    db::UserInfo::Ptr info(new db::UserInfo());
    info->uid = item["uid"].asInt64();
    info->name = item["name"].asString();
    info->age = item["age"].asInt();
    info->sex = item["sex"].asInt();
    info->headImageURL = item["headImageURL"].asString();
    friendMap[info->uid] = info;
  }

  LOG_INFO(wim::businessLogger, "from: {}, friendList size: {}", user->uid,
           friendMap.size());

  return !friendMap.empty();
}

bool Chat::pullFriendApplyListHandle(const Json::Value &response) {
  Json::Value tmp = response["applyList"];
  for (auto &item : tmp) {
    db::FriendApply::Ptr apply(new db::FriendApply());
    apply->fromUid = item["fromUid"].asInt64();
    apply->toUid = item["toUid"].asInt64();
    apply->status = item["status"].asInt();
    apply->content = item["content"].asString();
    apply->createTime = item["createTime"].asString();
    friendApplyMap[apply->toUid] = apply;
  }
  LOG_INFO(wim::businessLogger, "from: {}, applyList size: {}", user->uid,
           friendApplyMap.size());
  return !friendApplyMap.empty();
}
bool Chat::pullMessageListHandle(const Json::Value &response) {
  Json::Value tmp = response["messageList"];
  long toId = response["toId"].asInt64();
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
  messageListMap[toId] = messageList;
  LOG_INFO(wim::businessLogger, "from: {}, to: {}, messageList size: {}",
           user->uid, toId, messageList->size());
  return true;
}

bool Chat::notifyAddFriend(long uid, const std::string &requestMessage) {
  LOG_INFO(wim::businessLogger, "[Add Friend]uid: {}, request:  {}", uid,
           requestMessage);

  Json::Value addFriendReq;
  addFriendReq["fromUid"] = Json::Value::Int64(user->uid);
  addFriendReq["toUid"] = Json::Value::Int64(uid);
  addFriendReq["requestMessage"] = requestMessage;
  chat->Send(addFriendReq.toStyledString(), ID_NOTIFY_ADD_FRIEND_REQ);

  return true;
}
bool Chat::replyAddFriend(long uid, bool accept,
                          const std::string &replyMessage) {
  Json::Value addFriendRequest;
  addFriendRequest["fromUid"] = Json::Value::Int64(user->uid);
  addFriendRequest["toUid"] = Json::Value::Int64(uid);
  addFriendRequest["accept"] = accept;
  addFriendRequest["replyMessage"] = replyMessage;
  LOG_INFO(businessLogger, "request json: {}",
           addFriendRequest.toStyledString());

  chat->Send(addFriendRequest.toStyledString(), ID_REPLY_ADD_FRIEND_REQ);

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
  long seq = generateSequeueId();
  textMsg["seq"] = Json::Value::Int64(seq);
  textMsg["fromUid"] = Json::Value::Int64(user->uid);
  textMsg["toUid"] = Json::Value::Int64(uid);
  textMsg["text"] = message;
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
bool Chat::joinGroup(long groupId) {
  Json::Value request;
  request["groupId"] = Json::Value::Int64(groupId);
  request["uid"] = Json::Value::Int64(user->uid);
  chat->Send(request.toStyledString(), ID_GROUP_JOIN_REQ);
  return true;
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

bool Chat::createGroupHandle(Json::Value &response) {
  db::GroupManager info;

  info.gid = response["groupId"].asInt64();
  info.sessionKey = response["sessionKey"].asInt64();
  info.createTime = response["createTime"].asString();

  return true;
}
bool Chat::joinGroupHandle(Json::Value &response) { return true; }
bool Chat::applyJoinGroupHandle(Json::Value &response) {
  db::GroupMember member;
  member.uid = response["uid"].asInt64();
  member.memberName = response["name"].asString();
  member.joinTime = response["joinTime"].asString();
  member.role = response["role"].asInt();
  member.speech = response["speech"].asInt();

  return true;
}
bool Chat::quitGroupHandle(Json::Value &response) { return true; }
bool Chat::sendGroupTextMessageHandle(Json::Value &response) { return true; }
bool Chat::pullGroupMemberHandle(Json::Value &response) {

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
  return true;
}
bool Chat::pullGroupMessageHandle(Json::Value &response) { return true; }

// 文件
bool Chat::sendFile(long toId, const std::string &filePath) {
  Json::Value request;

  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", filePath);
    return false;
  }
  static const int chunkSize = 1024 * 1024; // 1MB
  static std::atomic<long> seq(3000);
  char buffer[chunkSize];

  db::File::Ptr fileInfo(new db::File());
  fileInfo->seq = seq;
  fileInfo->type = db::File::Type::FILE;
  fileInfo->name = filePath;

  fileMap[filePath] = fileInfo;

  while (!file.eof()) {
    file.read(buffer, chunkSize);
    fileInfo->offset += chunkSize;
    fileInfo->data.append(buffer, file.gcount());

    request["seq"] = Json::Value::Int64(seq);
    request["fromUid"] = Json::Value::Int64(user->uid);
    request["toId"] = Json::Value::Int64(toId);
    request["data"] = buffer;

    chat->Send(request.toStyledString(), ID_FILE_UPLOAD_REQ);

    // 客户端主动P99延迟，保顺序
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  seq++;
  file.close();
  return true;
}
bool Chat::sendFileHandle(Json::Value &response) {
  std::string name = response["name"].asString();
  long seq = response["seq"].asInt64();
  long toUid = response["toId"].asInt64();
  int status = response["status"].asInt();

  LOG_INFO(businessLogger, "recv file: {}, seq: {}, toUid: {}, status: {}",
           name, seq, toUid, status);
  return true;
}

bool Chat::recvFileHandle(Json::Value &response) {
  std::string name = response["name"].asString();
  long seq = response["seq"].asInt64();
  long fromUid = response["fromUid"].asInt64();
  std::string data = response["data"].asString();

  std::string savePath =
      "./" + std::to_string(user->uid) + "/files/" + name + ".txt";
  std::fstream file(savePath, std::ios::out | std::ios::binary | std::ios::app);
  if (!file.is_open()) {
    LOG_ERROR(businessLogger, "open file failed: {}", name);
    return false;
  }
  file.write(data.c_str(), data.size());
  file.close();

  // ack todo...
  return true;
}

}; // namespace wim