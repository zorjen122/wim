#include "Group.h"

#include "OnlineUser.h"
#include "Service.h"
#include <spdlog/spdlog.h>
namespace wim {
void GroupCreate(std::shared_ptr<ChatSession> session, unsigned int msgID,
                 const Json::Value &request) {

  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_CREATE_RSP);
  });

  // gid should by server generate, todo...
  int gid = request["gid"].asInt();
  int uid = request["uid"].asInt();

  Group group;
  group.id = gid;
  group.up = uid;
  dev::gg[gid] = group;

  rsp["error"] = ErrorCodes::Success;
  rsp["gid"] = gid;
}

void GroupJoin(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const Json::Value &request) {

  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_JOIN_RSP);
  });

  int gid = request["gid"].asInt();
  int fromID = request["from"].asInt();

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

// TEXT todo...
void GroupQuit(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const Json::Value &request) {
  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_JOIN_RSP);
  });
  int gid = request["gid"].asInt();
  int fromID = request["from"].asInt();

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
                   const Json::Value &request) {

  Json::Value rsp;

  Defer _([&rsp, session]() {
    std::string rt = rsp.toStyledString();
    session->Send(rt, ID_GROUP_TEXT_SEND_RSP);
  });

  int gid = request["gid"].asInt();
  int fromID = request["from"].asInt();
  std::string msg = request["text"].asString();

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

      PushText(toSession, util::seqGenerator, fromID, to,
               request.toStyledString(), ID_GROUP_TEXT_SEND_RSP);
    } else {
      rsp["seq"] = static_cast<int>(util::seqGenerator.load());
      rsp["status"] = "Unread-Pull";
      rsp["error"] = ErrorCodes::Success;

      PullText(util::seqGenerator, fromID, to, request.toStyledString());
    }

    OnRewriteTimer(session, seq, rsp.toStyledString(), ID_GROUP_TEXT_SEND_RSP,
                   fromID);
    ++util::seqGenerator;
  }
}
}; // namespace wim