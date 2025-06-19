#pragma once
#include "ChatSession.h"
#include <jsoncpp/json/value.h>

namespace wim {

Json::Value NotifyAddFriend(ChatSession::ptr session, unsigned int msgID,
                            Json::Value &request);
int StoreageNotifyAddFriend(Json::Value &request);

Json::Value ReplyAddFriend(ChatSession::ptr session, unsigned int msgID,
                           Json::Value &request);
int StoreageReplyAddFriend(Json::Value &request);

void RemoveFriend(ChatSession::ptr session, unsigned int msgID,
                  Json::Value &request);
}; // namespace wim