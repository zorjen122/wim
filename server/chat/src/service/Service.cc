#include "Service.h"
#include "ChatServer.h"
#include "ChatSession.h"
#include "Configer.h"
#include "Const.h"

#include "KafkaOperator.h"
#include "Logger.h"
#include "OnlineUser.h"
#include "Redis.h"

#include "Friend.h"
#include "Group.h"

#include "Logger.h"
#include "Mysql.h"
#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <jsoncpp/json/json.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

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

void Service::Init() {
  // 已成功
  serviceGroup[ID_LOGIN_INIT_REQ] =
      std::bind(&OnLogin, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_NOTIFY_ADD_FRIEND_REQ] =
      std::bind(wim::NotifyAddFriend, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  // 待测试

  serviceGroup[ID_PING_REQ] =
      std::bind(&PingHandle, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);
  serviceGroup[ID_TEXT_SEND_REQ] =
      std::bind(&TextSend, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_ACK] =
      std::bind(&AckHandle, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_GROUP_CREATE_REQ] =
      std::bind(&GroupCreate, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_GROUP_JOIN_REQ] =
      std::bind(&GroupJoin, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  serviceGroup[ID_GROUP_TEXT_SEND_REQ] =
      std::bind(&GroupTextSend, std::placeholders::_1, std::placeholders::_2,
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

        auto handleCall = serviceGroup.find(id);
        if (handleCall == serviceGroup.end()) {
          LOG_INFO(wim::businessLogger, "not found! service ID  is {}, ", id);
          messageQueue.pop();
          continue;
        }

        std::string msgData{data, dataSize};
        Json::Reader reader;
        Json::Value request;

        bool parserSuccess = reader.parse(msgData, request);
        if (!parserSuccess) {
          LOG_WARN(wim::businessLogger, "parse message data error!, id: {}",
                   id);
          Json::Value rsp;
          rsp["error"] = ErrorCodes::JsonParser;
          package->contextSession->Send(rsp.toStyledString(), id);
          return;
        }

        handleCall->second(package->contextSession, id, request);
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

void ClearChannel(size_t uid, ChatSession::Ptr session) {
  OnlineUser::GetInstance()->RemoveUser(uid);
  session->ClearSession();
  session->Close();
  LOG_DEBUG(wim::businessLogger,
            "[Service::ClearChannel] clear channel success, uid-{}", uid);
}

void Pong(int uid, ChatSession::Ptr session);

// 待验证
static std::map<int, std::shared_ptr<net::steady_timer>> waitAckTimerMap;

void onReWrite(int seq, ChatSession::Ptr session, const std::string &package,
               int rsp, int callcount = 0) {

  session->Send(package, rsp);
  auto waitAckTimer = std::make_shared<net::steady_timer>(session->GetIoc());
  waitAckTimerMap[seq] = waitAckTimer;
  waitAckTimer->expires_after(std::chrono::seconds(10));

  waitAckTimer->async_wait(
      [=, &waitAckTimer](const boost::system::error_code &ec) {
        if (ec == boost::asio::error::operation_aborted) {
          LOG_DEBUG(wim::businessLogger, "timer cancel");
          waitAckTimerMap.erase(seq);
          return;
        } else if (ec == boost::asio::error::timed_out) {
          static const int max_timeout_count = 3;
          if (callcount > max_timeout_count) {
            session->Close();
            return;
          }
          waitAckTimerMap.erase(seq);
          waitAckTimer.reset();
          onReWrite(seq, session, package, rsp, callcount + 1);
        }
      });
}

// 待废弃
int OnRewriteTimer(ChatSession::Ptr session, size_t seq, const std::string &rsp,
                   unsigned int rspID, unsigned int member,
                   unsigned int timewait, unsigned int maxRewrite) {

  util::retansfTimerMap[seq][member] =
      std::make_shared<net::steady_timer>(session->GetIoc());
  util::retansfCountMap[seq][member] = 0;
  auto &timer = util::retansfTimerMap[seq][member];

  auto lam = std::make_shared<std::function<void()>>();
  *lam = [=]() {
    if (timer == nullptr) {
      spdlog::error("[OnRewriteTimer] timer is nullptr");
      return;
    }
    timer->expires_from_now(std::chrono::seconds(timewait));

    timer->async_wait([=](const net::error_code &ec) {
      if (ec == net::error::operation_aborted) {
        LOG_DEBUG(wim::businessLogger,
                  "[OnRewriteTimer] timer by cancelled | seq {}, member {}",
                  seq, member);

        switch (rspID) {
        case ID_PING_REQ:
        case ID_PING_RSP: {
          // 暂行：目前因为全局seq分配无锁须顺行处理
          Pong(member, session);
          break;
        }
        }

        util::retansfTimerMap[seq].erase(member);
        util::retansfCountMap[seq].erase(member);

        return;
      } else if (ec.value() == 0) {

        // timer timeout, retransf package
        if (util::retansfCountMap[seq][member] == maxRewrite) {
          LOG_DEBUG(wim::businessLogger,
                    "[OnRewriteTimer] retransf count exceed 3 | seq-id "
                    "[{}],member [{}]",
                    seq, member);

          switch (rspID) {
          case ID_PING_REQ:
          case ID_PING_RSP: {
            ClearChannel(member, session);
            break;
          }
          }

          util::retansfTimerMap[seq].erase(member);
          util::retansfCountMap[seq].erase(member);
          return;
        }

        LOG_DEBUG(wim::businessLogger,
                  "[OnRewriteTimer] timeout, on rewrite package | "
                  "seq-id [{}], count [{}]",
                  seq, util::retansfCountMap[seq][member] + 1);

        session->Send(rsp, rspID);
        util::retansfCountMap[seq][member]++;
        (*lam)();
      } else {
        LOG_DEBUG(wim::businessLogger,
                  "[OnRewriteTimer] other timer click cased,  | ec: {}",
                  ec.message());

        util::retansfTimerMap[seq].erase(member);
        util::retansfCountMap[seq].erase(member);
      }
    });
  };

  (*lam)();

  LOG_DEBUG(wim::businessLogger,
            "[OnRewriteTimer] start rewrite timer, seq-id{}, member-{}", seq,
            member);
  return 0;
}

void Pong(int uid, ChatSession::Ptr session) {
  Json::Value pong;
  int seq = static_cast<int>(util::seqGenerator.load());
  pong["seq"] = seq;
  pong["uid"] = uid;
  pong["error"] = ErrorCodes::Success;

  session->Send(pong.toStyledString(), ID_PING_RSP);
  OnRewriteTimer(session, seq, pong.toStyledString(), ID_PING_RSP, uid);
  util::seqGenerator++;

  LOG_DEBUG(wim::businessLogger, "[ PONG | seq {}, uid {} ]", seq, uid);
}

void PingHandle(ChatSession::Ptr session, unsigned int msgID,
                const Json::Value &request) {
  spdlog::info("ping handle");
}

int PushText(ChatSession::Ptr toSession, size_t seq, int from, int to,
             const std::string &msg, int msgID) {

  toSession->Send(msg, msgID);

  return OnRewriteTimer(toSession, seq, msg, msgID, to);
}

bool SaveService(size_t seq, int from, int to, std::string msg) {
  auto file = open("./service.log", O_CREAT | O_RDWR | O_APPEND);
  char buf[4096]{};
  int rt = sprintf(buf, "From-%d|To-%d\t%s\n", from, to, msg.c_str());
  if (rt < 0) {
    spdlog::error("[ServiceSystem::SaveService] sprintf error");
    return false;
  }

  rt = write(file, buf, strlen(buf));

  if (rt < 0) {
    spdlog::error("[ServiceSystem::SaveService] write file error");
    return false;
  } else {
    LOG_DEBUG(wim::businessLogger,
              "[ServiceSystem::SaveService] save service log success");
  }
  close(file);

  return true;
}

bool SaveServiceDB(size_t seq, int from, int to, const std::string &msg) {
  // todo...
}

int PullText(size_t seq, int from, int to, const std::string &msg) {

  return SaveService(util::seqGenerator, from, to, msg);
  // return SaveServiceDB(util::seqGenerator, from, to, msg);
}

// 待验证
void TextSendAck(ChatSession::Ptr session, unsigned int msgID,
                 const Json::Value &msgData) {
  Json::Reader parser{};
  Json::Value req{};

  int seq = req["seq"].asInt();
  if (waitAckTimerMap.find(seq) == waitAckTimerMap.end()) {
    spdlog::warn(
        "[ServiceSystem::TextSendAck] no wait ack timer found for seq-id [{}]",
        seq);
    return;
  };

  waitAckTimerMap[seq]->cancel();
}

// 待废弃
void AckHandle(ChatSession::Ptr session, unsigned int msgID,
               const Json::Value &msgData) {
  Json::Reader parser{};
  Json::Value req{};
  Json::Value rsp{};

  Defer _([&rsp, session]() {
    auto rt = rsp.toStyledString();
    session->Send(rt, ID_ACK);
  });

  rsp.append(req);
  auto seq = req["seq"].asInt();
  if (util::retansfTimerMap.find(seq) == util::retansfTimerMap.end()) {
    spdlog::error(
        "[ServiceSystem::AckHandle] no retransf timer found for seq-id [{}]",
        seq);
    rsp["error"] = -1;
    return;
  }
  if (util::retansfCountMap.find(seq) == util::retansfCountMap.end()) {
    spdlog::error(
        "[ServiceSystem::AckHandle] retransf count not found for seq-id [{}]",
        seq);
    rsp["error"] = -1;
    return;
  }

  auto member = req["from"].asInt();
  auto reqID = req["id"].asInt();
  switch (reqID) {} // todo...

  auto &timer = util::retansfTimerMap[seq][member];
  timer->cancel();
  util::retansfTimerMap[seq].erase(member);
  util::retansfCountMap[seq].erase(member);
  rsp["error"] = ErrorCodes::Success;
  LOG_DEBUG(
      wim::businessLogger,
      "[ServiceSystem::AckHandle] ack handle success, seq-id{}, member-{}", seq,
      member);

  // rsp["seq"] = seq;
  // rsp["status"] = "Read";
  // rsp["error"] = ErrorCodes::Success;
  // util::sChannel[session]->Send(rsp.toStyledString(),
  // ID_TEXT_CHAT_MSG_RSP);

  // 若客户端发送了ack，服务端仍没收到，此时超时重发的消息会被客户端读取并分析处理
  // ，截止服务端收到ack，其中客户端包含ack重发次数逻辑，因此无需再发送
}

void UserSearch(ChatSession::Ptr session, unsigned int msgID,
                const Json::Value &request) {

  Json::Value response;

  Defer _([&response, session]() {
    std::string rt = response.toStyledString();
    session->Send(rt, ID_SEARCH_USER_RSP);
  });
  int uid = request["uid"].asInt();
  // todo... hasUser(uid) function
  // bool hasUser = wim::db::MysqlDao::GetInstance()->UserSerach(uid);
  // todo... get user [info] from redis or stack/heap
  bool isOnline = OnlineUser::GetInstance()->isOnline(uid);
  if (isOnline) {
    response["error"] = ErrorCodes::Success;
    response["uid"] = uid;
    LOG_DEBUG(wim::businessLogger, "[ServiceSystem::SearchUser user-{} online",
              uid);
  }

  bool hasUser = true;
  if (!hasUser) {
    response["error"] = ErrorCodes::UserNotOnline;
    response["uid"] = -1;
    spdlog::error("[ServiceSystem::SearchUser] no this a user, user-{}", uid);
    return;
  }

  response["error"] = ErrorCodes::Success;
  response["uid"] = uid;
  LOG_DEBUG(wim::businessLogger, "[ServiceSystem::SearchUser] users-{}", uid);
  OnRewriteTimer(session, util::seqGenerator, response.toStyledString(),
                 ID_SEARCH_USER_RSP, 0);
  ++util::seqGenerator;
}

// 待验证
void TextSend(ChatSession::Ptr session, unsigned int msgID,
              const Json::Value &senderReq) {

  Json::Value senderRsp;

  Defer _([&senderRsp, session]() {
    std::string rt = senderRsp.toStyledString();
    session->Send(rt, ID_TEXT_SEND_RSP);
  });

  auto from = senderReq["fromUid"].asInt();
  auto to = senderReq["toUid"].asInt();
  std::string text = senderReq["text"].asString();
  LOG_INFO(businessLogger, "from: {}, to: {}, text: {}", from, to, text);
  return;
  // todo...  待实现

  static std::set<int> senderIdCache{};
  if (senderIdCache.find(from) == senderIdCache.end()) {
    senderIdCache.insert(from);
  } else {
    return;
  }

  bool isOnline = OnlineUser::GetInstance()->isOnline(to);
  int seq = 0;
  if (isOnline) {
    senderRsp["status"] = "unread";
    senderRsp["error"] = ErrorCodes::Success;

    Json::Value recvierRsp;
    recvierRsp["seq"] = seq;
    recvierRsp.append(senderReq);
    auto reciverSession = OnlineUser::GetInstance()->GetUserSession(to);
    onReWrite(seq, reciverSession, recvierRsp.toStyledString(),
              ID_TEXT_SEND_REQ);
  } else {
  }
}
void UserQuit(ChatSession::Ptr session, unsigned int msgID,
              const Json::Value &request) {
  Json::Value rsp;
  Defer defer([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_USER_QUIT_WAIT_RSP);
  });

  auto uid = request["uid"].asInt();

  auto userSession = OnlineUser::GetInstance()->GetUserSession(uid);
  userSession->ClearSession();
  OnlineUser::GetInstance()->RemoveUser(uid);

  // bug-maybe: 需要确保完整发送后，再关闭连接，否则会导致客户端收不到消息
  userSession->Close();
}

void ReLogin(int uid, ChatSession::Ptr oldSession,
             ChatSession::Ptr newSession) {
  Json::Value info;
  info["uid"] = uid;
  info["status"] = "close";

  // todo...  需要可靠通知机制
  oldSession->Send(info.toStyledString(), ID_LOGIN_SQUEEZE);
  ClearChannel(uid, oldSession);
}

void OnLogin(ChatSession::Ptr session, unsigned int msgID,
             const Json::Value &request) {
  Json::Value rsp;
  long uid = request["uid"].asInt64();
  int status = 0;

  Defer _([&]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_LOGIN_INIT_RSP);
  });

  // 待实现，先不做处理
  status = OnlineUser::GetInstance()->isOnline(uid);
  if (false && status == false) {

    rsp["error"] = ErrorCodes::UserOnline;
    auto oldSession = OnlineUser::GetInstance()->GetUserSession(uid);
    ReLogin(uid, oldSession, session);

    LOG_DEBUG(wim::businessLogger,
              "[ServiceSystem::LoginHandle] THIS USER IS ONLINE, uid-{}", uid);
    return;
  }

  status = db::RedisDao::GetInstance()->getOnlineUserInfo(uid).empty();
  // 分布式情况，待实现
  if (false && status) {
    rsp["error"] = ErrorCodes::Success;
    return;
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
      LOG_DEBUG(wim::businessLogger, "insert user info failed, uid-{} ", uid);
      rsp["error"] = -1;
      return;
    }
  } else {
    userInfo = db::MysqlDao::GetInstance()->getUserInfo(uid);
    if (userInfo == nullptr) {
      LOG_DEBUG(wim::businessLogger, "get user info failed, uid-{} ", uid);
      rsp["error"] = -1;
      return;
    }
  }

  // 建立<userInfo, session>用户网络线路映射
  status = OnlineUser::GetInstance()->MapUser(userInfo, session);
  if (status == false) {
    rsp["error"] = -1;
    return;
  }

  rsp["uid"] = Json::Value::Int64(userInfo->uid);
  rsp["name"] = userInfo->name;
  rsp["age"] = userInfo->age;
  rsp["sex"] = userInfo->sex;
  rsp["headImageURL"] = userInfo->headImageURL;
  rsp["error"] = ErrorCodes::Success;

  LOG_DEBUG(wim::businessLogger, "user login success!, uid: {}, response: {} ",
            uid, rsp.toStyledString());
}
}; // namespace wim