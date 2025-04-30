#pragma once
#include "ChatSession.h"

#include <jsoncpp/json/value.h>
#include <string>

namespace wim {

Json::Value NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                            Json::Value &request);
int OnlineNotifyAddFriend(ChatSession::Ptr user, Json::Value &request);
int OfflineNotifyAddFriend(Json::Value &request);

Json::Value ReplyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                           Json::Value &request);
int OnlineReplyAddFriend(ChatSession::Ptr user, Json::Value &request);
int OfflineReplyAddFriend(Json::Value &request);

int OnlineRemoveFriend(long from, long to, ChatSession::Ptr toSession);
int OfflineRemoveFriend(long from, long to, const std::string &msgData);

void RemoveFriend(ChatSession::Ptr session, unsigned int msgID,
                  Json::Value &request);
}; // namespace wim