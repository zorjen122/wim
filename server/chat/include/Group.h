#pragma once

#include "Channel.h"
#include "ChatSession.h"
#include <cstddef>
#include <jsoncpp/json/json.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wim {

Json::Value GroupCreate(ChatSession::Ptr session, unsigned int msgID,
                        Json::Value &request);
Json::Value GroupNotifyJoin(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request);

Json::Value GroupPullNotify(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request);

Json::Value GroupReplyJoin(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request);
Json::Value GroupQuit(ChatSession::Ptr session, unsigned int msgID,
                      Json::Value &request);
Json::Value GroupTextSend(ChatSession::Ptr session, unsigned int msgID,
                          Json::Value &request);
}; // namespace wim