#pragma once
#include "ChatSession.h"
#include <jsoncpp/json/value.h>

namespace wim {

Json::Value NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request);
int StoreageNotifyAddFriend(Json::Value &request);

Json::Value ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request);
int StoreageReplyAddFriend(Json::Value &request);

void RemoveFriend(ChatSession::Ptr session, unsigned int msgID,
                  Json::Value &request);
}; // namespace wim