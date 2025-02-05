#include "Service.h"
#include "ChatServer.h"
#include "ChatSession.h"
#include "Configer.h"
#include "Const.h"

#include "OnlineUser.h"
#include "RedisManager.h"
#include "RpcClient.h"
#include "SqlOperator.h"
#include "json/reader.h"
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
#include <string>
#include <unordered_map>

Service::Service() : isStop(false) {
  Init();
  worker = std::thread(&Service::Run, this);
}

Service::~Service() {
  isStop = true;
  _consume.notify_one();
  worker.join();
}

namespace util {
static std::atomic<size_t> seqGenerator(1);
static std::unordered_map<
    size_t, std::unordered_map<size_t, std::unique_ptr<net::steady_timer>>>
    retansfTimerMap;

static std::unordered_map<size_t, std::unordered_map<size_t, size_t>>
    retansfCountMap;
static std::unordered_map<std::shared_ptr<ChatSession>,
                          std::shared_ptr<ChatSession>>
    sChannel;
} // namespace util

int OnRewriteTimer(std::shared_ptr<ChatSession> session, size_t seq,
                   const std::string &rsp, unsigned int rspID,
                   unsigned int member) {

  util::retansfTimerMap[seq][member] =
      std::make_unique<net::steady_timer>(session->GetIoc());
  util::retansfCountMap[seq][member] = 0;

  std::function<void()> lam{};
  lam = [session, rsp, seq, rspID, member, lam]() {
    auto &timer = util::retansfTimerMap[seq][member];
    timer->expires_from_now(std::chrono::seconds(5));

    timer->async_wait([&](const error_code &ec) {
      if (ec == net::error::operation_aborted) {
        spdlog::info("[OnRewriteTimer] timer by cancelled");
      } else if (ec.value() == 0) {
        // timer timeout, retransf package
        if (util::retansfCountMap[seq][member] > 3) {
          spdlog::error("[OnRewriteTimer] retransf count exceed 3");
          util::retansfTimerMap[seq].erase(member);
          util::retansfCountMap[seq].erase(member);

          switch (rspID) {}; // todo
          return;
        }
        spdlog::info("[OnRewriteTimer] timeout, on rewrite package, "
                     "seq-id [{}], count [{}]",
                     seq, util::retansfCountMap[seq][member]);

        session->Send(rsp, rspID);
        util::retansfCountMap[seq][member]++;
        lam();
      } else {
        spdlog::info(
            "[OnRewriteTimer] other timer click cased,  | ec: {}, msg: {}",
            ec.value(), ec.message());
        util::retansfTimerMap[seq].erase(member);
        util::retansfCountMap[seq].erase(member);
      }
    });
  };

  lam();

  spdlog::info("[OnRewriteTimer] start rewrite timer, seq-id{}, member-{}", seq,
               member);
  return 0;
}

int PushText(std::shared_ptr<ChatSession> toSession, size_t seq, int from,
             int to, const std::string &msg, int msgID = ID_TEXT_CHAT_MSG_RSP) {

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
    spdlog::info("[ServiceSystem::SaveService] save service log success");
  }
  close(file);

  return true;
}

bool SaveServiceDB(size_t seq, int from, int to, const std::string &msg) {
  return MySqlOperator::GetInstance()->SaveService(from, to, msg);
}

int PullText(size_t seq, int from, int to, const std::string &msg) {

  return SaveService(util::seqGenerator, from, to, msg);
  // return SaveServiceDB(util::seqGenerator, from, to, msg);
}

void AckHandle(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData) {
  Json::Reader parser{};
  Json::Value req{};
  Json::Value rsp{};
  bool parserSuccess = parser.parse(msgData, req);

  Defer _([&rsp, session]() {
    auto rt = rsp.toStyledString();
    session->Send(rt, ID_UTIL_ACK_RSP);
  });

  if (!parserSuccess) {
    spdlog::error("[ServiceSystem::AckHandle] parse msg_data error!");
    rsp["error"] = ErrorCodes::JsonParserErr;
    return;
  }

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
  auto &timer = util::retansfTimerMap[seq][member];
  timer->cancel();
  util::retansfTimerMap[seq].erase(member);
  util::retansfCountMap[seq].erase(member);
  rsp["error"] = ErrorCodes::Success;
  spdlog::info(
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

int OnlineAddFriend(int seq, int from, int to,
                    std::shared_ptr<ChatSession> toSession) {
  if (toSession == nullptr) {
    spdlog::error("[ServiceSystem::OnlineAddFriend] toSession is nullptr");
    return -1;
  }

  Json::Value rsp{};
  rsp["seq"] = seq;
  rsp["from"] = from;
  rsp["to"] = to;
  rsp["error"] = ErrorCodes::Success;

  // todo
  // MySqlOperator::GetInstance()->AddFriend(from, to);

  // 当客户端接收到ID_ADD_FRIEND_REQ时，意味着这只能是服务端的消息
  toSession->Send(rsp.toStyledString(), ID_ADD_FRIEND_REQ);
  OnRewriteTimer(toSession, seq, rsp.toStyledString(), ID_ADD_FRIEND_REQ, to);

  return 0;
}

int OfflineAddFriend(int seq, int from, int to, const std::string &msgData) {
  spdlog::info("[ServiceSystem::OfflineAddFriend] seq-{}, from-{}, to-{}", seq,
               from, to);
  int rt = MySqlOperator::GetInstance()->SaveService(from, to, msgData);
  if (rt == false) {
    spdlog::error("OffineAddFriend save service to mysql failed");
    return -1;
  }

  return 0;
}

void AddFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_ADD_FRIEND_RSP);
  });

  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::AddFriend] parse msg_data error!");
    return;
  }

  int from = req["from"].asInt();
  int to = req["to"].asInt();

  // bool hasUser = MySqlOperator::GetInstance()->HasUser(to);
  bool hasUser = true;
  if (!hasUser) {
    rsp["uid"] = to;
    rsp["error"] = ErrorCodes::UserNotOnline;
    rsp["status"] = "x";
    spdlog::error("[ServiceSystem::AddFriend] no this a user, user-{}", to);
    return;
  }

  bool isOnline = OnlineUser::GetInstance()->isOnline(to);

  rsp["uid"] = to;
  rsp["error"] = ErrorCodes::Success;
  rsp["status"] = "wait";
  if (isOnline) {
    auto toSession = OnlineUser::GetInstance()->GetUser(to);
    int rt = OnlineAddFriend(util::seqGenerator, from, to, toSession);
    if (rt == -1) {
      rsp["error"] = -1;
      return;
    }
  } else {
    int rt = OfflineAddFriend(util::seqGenerator, from, to, msgData);
    if (rt == -1) {
      rsp["error"] = -1;
    }
  }

  OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                 ID_ADD_FRIEND_RSP, from);
  ++util::seqGenerator;
}

int OnlineRemoveFriend(int seq, int from, int to,
                       std::shared_ptr<ChatSession> toSession) {
  if (toSession == nullptr) {
    spdlog::error("[ServiceSystem::OnlineRemoveFriend] toSession is nullptr");
    return -1;
  }

  // todo... remove friend in mysql
  bool isRemoved = MySqlOperator::GetInstance()->RemovePair(from, to);

  if (isRemoved) {
    Json::Value notify{};
    notify["seq"] = seq;
    notify["from"] = from;
    notify["to"] = to;
    toSession->Send(notify.toStyledString(), ID_REMOVE_FRIEND_REQ);
    OnRewriteTimer(toSession, seq, notify.toStyledString(),
                   ID_REMOVE_FRIEND_REQ, to);
  }

  spdlog::error(
      "[ServiceSystem::OnlineRemoveFriend] mysql remove friend failed");
  return -1;
}

int OfflineRemoveFriend(int seq, int from, int to, const std::string &msgData) {

  spdlog::info("[ServiceSystem::OfflineRemoveFriend] seq-{}, from-{}, to-{}",
               seq, from, to);
  int rt = MySqlOperator::GetInstance()->SaveService(from, to, msgData);
  if (rt == false) {
    spdlog::error(
        "[ServiceSystem::OffineAddFriend] save service to mysql failed");
    return -1;
  }

  return 0;
}

void RemoveFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_ADD_FRIEND_RSP);
  });

  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::RemoveFriend] parse msg_data error!");
    return;
  }

  int from = req["from"].asInt();
  int to = req["to"].asInt();

  // todo... hasFriend function in mysql
  // bool hasFriend = MySqlOperator::GetInstance()->PairSearch(from, to);
  bool hasFriend = true;
  if (!hasFriend) {
    rsp["uid"] = to;
    rsp["error"] = ErrorCodes::UserNotFriend;
    rsp["status"] = "x";
    spdlog::error("[ServiceSystem::RemoveFriend] no this a user, user-{}", to);
    return;
  }

  bool isOnline = OnlineUser::GetInstance()->isOnline(to);

  rsp["uid"] = to;
  rsp["error"] = ErrorCodes::Success;
  rsp["status"] = "wait";
  if (isOnline) {
    auto toSession = OnlineUser::GetInstance()->GetUser(to);
    int rt = OnlineRemoveFriend(util::seqGenerator, from, to, toSession);
    if (rt == -1) {
      rsp["error"] = -1;
      return;
    }
  } else {
    int rt = OfflineRemoveFriend(util::seqGenerator, from, to, msgData);
    if (rt == -1) {
      rsp["error"] = -1;
    }
  }

  OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                 ID_ADD_FRIEND_RSP, from);
  ++util::seqGenerator;
}

void UserSearch(std::shared_ptr<ChatSession> session, unsigned int msgID,
                const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::SearchUser] parse msg_data error!");
    return;
  }
  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_SEARCH_USER_RSP);
  });
  int uid = req["uid"].asInt();
  // todo... hasUser(uid) function
  // bool hasUser = MySqlOperator::GetInstance()->UserSerach(uid);
  // todo... get user [info] from redis or stack/heap
  bool isOnline = OnlineUser::GetInstance()->isOnline(uid);
  if (isOnline) {
    rsp["error"] = ErrorCodes::Success;
    rsp["uid"] = uid;
    spdlog::info("[ServiceSystem::SearchUser user-{} online", uid);
  }

  bool hasUser = true;
  if (!hasUser) {
    rsp["error"] = ErrorCodes::UserNotOnline;
    rsp["uid"] = -1;
    spdlog::error("[ServiceSystem::SearchUser] no this a user, user-{}", uid);
    return;
  }

  rsp["error"] = ErrorCodes::Success;
  rsp["uid"] = uid;
  spdlog::info("[ServiceSystem::SearchUser] users-{}", uid);
  OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                 ID_SEARCH_USER_RSP, 0);
  ++util::seqGenerator;
}

void TextSend(std::shared_ptr<ChatSession> session, unsigned int msgID,
              const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;

  auto parseSuccess = parser.parse(msgData, req);
  if (!parseSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
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

    PushText(toSession, util::seqGenerator, from, to, req.toStyledString());
  } else {
    rsp["seq"] = static_cast<int>(util::seqGenerator.load());
    rsp["status"] = "Unread-Pull";
    rsp["error"] = ErrorCodes::Success;

    PullText(util::seqGenerator, from, to, msgData);
  }

  OnRewriteTimer(session, seq, rsp.toStyledString(), ID_TEXT_CHAT_MSG_RSP,
                 from);
  ++util::seqGenerator;
}

struct GroupType {
  size_t id;
  size_t up;
  std::vector<size_t> numbers;
};

namespace dev {
std::unordered_map<size_t, GroupType> gg;
};

void GroupCreate(std::shared_ptr<ChatSession> session, unsigned int msgID,
                 const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::GroupCreate] parse msg_data error!");
    return;
  }

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_CREATE_RSP);
  });

  // gid should by server generate, todo...
  int gid = req["gid"].asInt();
  int uid = req["uid"].asInt();

  GroupType group;
  group.id = gid;
  group.up = uid;
  dev::gg[gid] = group;

  rsp["error"] = ErrorCodes::Success;
  rsp["gid"] = gid;
}

void GroupJoin(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::GroupJoin] parse msg_data error!");
    return;
  }

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_JOIN_RSP);
  });

  int gid = req["gid"].asInt();
  int fromID = req["from"].asInt();

  if (dev::gg.find(gid) == dev::gg.end()) {
    spdlog::error("[ServiceSystem::GroupJoin] group not found gid-{}", gid);
    rsp["error"] = -1;
    return;
  }

  auto &group = dev::gg[gid];
  if (std::find(group.numbers.begin(), group.numbers.end(), fromID) !=
      group.numbers.end()) {
    spdlog::error("[ServiceSystem::GroupJoin] This user has joined this group, "
                  "user-{}, group-{}",
                  fromID, gid);
    rsp["error"] = -1;
  } else {
    group.numbers.push_back(fromID);
    spdlog::info("[ServiceSystem::GroupJoin] user-{} join group-{}", fromID,
                 gid);
    rsp["error"] = ErrorCodes::Success;
    rsp["gid"] = gid;
  }
}

void GroupQuit(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::GroupQuit] parse msg_data error!");
    return;
  }
  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_JOIN_RSP);
  });
  int gid = req["gid"].asInt();
  int fromID = req["from"].asInt();

  int NOT_FOUND_GROUP = -100;
  int NOT_JOIN_GROUP_FOR_USER = -101;

  if (dev::gg.find(gid) == dev::gg.end()) {
    spdlog::error("[ServiceSystem::GroupQuit] group not found gid-{}", gid);
    rsp["error"] = NOT_FOUND_GROUP;
    return;
  }

  auto &group = dev::gg[gid];
  if (std::find(group.numbers.begin(), group.numbers.end(), fromID) ==
      group.numbers.end()) {
    spdlog::error("This user has not joined this group, user-{}, group-{}",
                  fromID, gid);
    spdlog::info("group-members: {");
    for (auto &number : group.numbers)
      spdlog::info("{}, ", number);
    spdlog::info("}");
    rsp["error"] = NOT_JOIN_GROUP_FOR_USER;
    return;
  }

  group.numbers.erase(
      std::remove(group.numbers.begin(), group.numbers.end(), fromID),
      group.numbers.end());
  spdlog::info("[ServiceSystem::GroupQuit] user-{} quit group-{}", fromID, gid);
  rsp["error"] = ErrorCodes::Success;
  rsp["gid"] = gid;
}

void GroupTextSend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                   const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::GroupTextSend] parse msgData error!");
    return;
  }

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_TEXT_SEND_RSP);
  });
  int gid = req["gid"].asInt();
  int fromID = req["from"].asInt();
  std::string msg = req["text"].asString();

  if (dev::gg.find(gid) == dev::gg.end()) {
    spdlog::error("[ServiceSystem::GroupTextSend] group not found gid-{}", gid);
    rsp["error"] = -1;
    return;
  };

  auto &group = dev::gg[gid];
  if (std::find(group.numbers.begin(), group.numbers.end(), fromID) ==
      group.numbers.end()) {
    spdlog::error("This user has not joined this group, user-{}, group-{}",
                  fromID, gid);
    spdlog::info("group-members: {");
    for (auto &number : group.numbers)
      spdlog::info("{}, ", number);
    spdlog::info("}");
    rsp["error"] = -1;
    return;
  }

  auto &numbers = group.numbers;
  for (auto to : numbers) {

    if (to == fromID)
      continue;

    bool isOnline = OnlineUser::GetInstance()->isOnline(to);
    int seq = static_cast<int>(util::seqGenerator.load());
    if (isOnline) {
      rsp["seq"] = seq;
      rsp["status"] = "Unread-Push";
      rsp["error"] = ErrorCodes::Success;

      auto toSession = OnlineUser::GetInstance()->GetUser(to);
      util::sChannel[toSession] = session;

      PushText(toSession, util::seqGenerator, fromID, to, req.toStyledString(),
               ID_GROUP_TEXT_SEND_RSP);
    } else {
      rsp["seq"] = static_cast<int>(util::seqGenerator.load());
      rsp["status"] = "Unread-Pull";
      rsp["error"] = ErrorCodes::Success;

      PullText(util::seqGenerator, fromID, to, req.toStyledString());
    }

    OnRewriteTimer(session, seq, rsp.toStyledString(), ID_GROUP_TEXT_SEND_RSP,
                   fromID);
    ++util::seqGenerator;
  }
}

void Service::Init() {
  _serviceGroup[ID_CHAT_LOGIN_INIT] =
      std::bind(&Service::LoginHandler, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_PING_PONG_REQ] =
      std::bind(&Service::PingKeepAlive, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_SEARCH_USER_REQ] =
      std::bind(&Service::SearchUser, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_ADD_FRIEND_REQ] =
      std::bind(&Service::AddFriendApply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_AUTH_FRIEND_REQ] =
      std::bind(&Service::AuthFriendApply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_PUSH_TEXT_MSG_REQ] =
      std::bind(&Service::PushTextMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_ONLINE_PULL_REQ] =
      std::bind(&Service::OnlinePullHandler, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_TEXT_SEND_REQ] =
      std::bind(&TextSend, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  _serviceGroup[ID_UTIL_ACK_SEQ] =
      std::bind(&AckHandle, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  _serviceGroup[ID_GROUP_CREATE_REQ] =
      std::bind(&GroupCreate, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  _serviceGroup[ID_GROUP_JOIN_REQ] =
      std::bind(&GroupJoin, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);

  _serviceGroup[ID_GROUP_TEXT_SEND_REQ] =
      std::bind(&GroupTextSend, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);
}

void Service::PingKeepAlive(std::shared_ptr<ChatSession> session,
                            unsigned int msgID, const std::string &msgData) {}

void Service::PushService(std::shared_ptr<protocol::LogicPackage> msg) {
  std::unique_lock<std::mutex> lock(_mutex);
  _messageGroup.push(msg);
  if (_messageGroup.size() == 1) {
    lock.unlock();
    _consume.notify_one();
  }
}

void Service::Run() {
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

void ReLogin(int uid, std::shared_ptr<ChatSession> userSession) {
  Json::Value info;
  info["uid"] = uid;

  // todo...  需要可靠通知机制
  userSession->Send(info.toStyledString(), ID_LOGIN_SQUEEZE);
  userSession->ClearSession();
  userSession->Close();
  OnlineUser::GetInstance()->RemoveUser(uid);
  OnlineUser::GetInstance()->MapUser(uid, userSession);
}

void UserQuitWait(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const std::string &msgData) {
  Json::Reader reader;
  Json::Value req;
  Json::Value rsp;

  reader.parse(msgData, req);
  auto uid = req["uid"].asInt();
  spdlog::info("[ServiceSystem::UserQuit] user quit uid is  ", uid);

  Defer defer([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_USER_QUIT_WAIT_RSP);
  });

  if (!OnlineUser::GetInstance()->isOnline(uid)) {
    rsp["error"] = ErrorCodes::UserOffline;
    spdlog::error("[ServiceSystem::UserQuit] user is offline, uid-{}", uid);
    return;
  } else {
    rsp["error"] = ErrorCodes::Success;
    rsp["status"] = "wait";

    OnRewriteTimer(session, util::seqGenerator, rsp.toStyledString(),
                   ID_USER_QUIT_WAIT_RSP, uid);
    ++util::seqGenerator;
  }
}

void UserQuit(std::shared_ptr<ChatSession> session, unsigned int msgID,
              const std::string &msgData) {
  Json::Reader reader;
  Json::Value req;
  Json::Value rsp;

  Defer defer([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_USER_QUIT_WAIT_RSP);
  });

  bool parserSuccess = reader.parse(msgData, req);
  if (!parserSuccess) {
    spdlog::error("[ServiceSystem::UserQuit] parse msgData error!");
    rsp["error"] = ErrorCodes::JsonParserErr;
    return;
  }

  auto uid = req["uid"].asInt();

  auto userSession = OnlineUser::GetInstance()->GetUser(uid);
  userSession->ClearSession();
  OnlineUser::GetInstance()->RemoveUser(uid);

  // bug: 需要确保完整发送后，再关闭连接，否则会导致客户端收不到消息
  userSession->Close();
}

void Service::LoginHandler(std::shared_ptr<ChatSession> session,
                           unsigned int msgID, const std::string &msgData) {
  Json::Reader parser;
  Json::Value req;
  Json::Value rsp;
  bool parserSuccess = parser.parse(msgData, req);
  if (!parserSuccess) {
    rsp["error"] = ErrorCodes::JsonParserErr;
    spdlog::error("[ServiceSystem::LoginHandle] parse msg_data error!");
    return;
  }

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_CHAT_LOGIN_INIT_RSP);
  });

  auto uid = req["uid"].asInt();
  // auto token = req["token"].asString();

  if (OnlineUser::GetInstance()->isOnline(uid)) {
    spdlog::info("[ServiceSystem::LoginHandle] THIS USER IS ONLINE, uid-{}",
                 uid);
    rsp["error"] = ErrorCodes::UserOnline;

    auto userSession = OnlineUser::GetInstance()->GetUser(uid);
    ReLogin(uid, userSession);

    return;
  } else {
    rsp["error"] = ErrorCodes::Success;
    // 应用层上的用户管理，而在ChatServer中则是对传输层连接的管理
    OnlineUser::GetInstance()->MapUser(uid, session);
    spdlog::info(
        "[ServiceSystem::LoginHandle] user logining, <uid: {} => session >",
        uid);
    return;
  }

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

void Service::SearchUser(std::shared_ptr<ChatSession> session,
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

void Service::AddFriendApply(std::shared_ptr<ChatSession> session,
                             unsigned int msgID, const std::string &msgData) {
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

void Service::AuthFriendApply(std::shared_ptr<ChatSession> session,
                              unsigned int msg_id,
                              const std::string &msg_data) {
  // todo
}
void Service::PushTextMessage(std::shared_ptr<ChatSession> session,
                              unsigned int msgID, const std::string &msgData) {
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

void Service::OnlinePullHandler(std::shared_ptr<ChatSession> session,
                                unsigned int msgID,
                                const std::string &msgData) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msgData, root);

  auto uid = root["uid"].asInt();
  // todo: online pull logic
}