#pragma once

#include "ChatSession.h"
#include <cstddef>
#include <cstdint>
#include <jsoncpp/json/json.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wim {

Json::Value GroupCreate(ChatSession::ptr session, unsigned int msgID,
                        Json::Value &request);
Json::Value GroupNotifyJoin(ChatSession::ptr session, unsigned int msgID,
                            Json::Value &request);

Json::Value GroupPullNotify(ChatSession::ptr session, unsigned int msgID,
                            Json::Value &request);

Json::Value GroupReplyJoin(ChatSession::ptr session, unsigned int msgID,
                           Json::Value &request);
Json::Value GroupQuit(ChatSession::ptr session, unsigned int msgID,
                      Json::Value &request);
Json::Value GroupTextSend(ChatSession::ptr session, unsigned int msgID,
                          Json::Value &request);
int NotifyMemberJoin(int64_t uid, int64_t gid,
                     const std::string &requestMessage);
int NotifyMemberReply(int64_t gid, int64_t managerUid, int64_t requestorUid,
                      bool accept);
}; // namespace wim