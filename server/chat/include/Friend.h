#pragma once
#include "Channel.h"
#include "ChatSession.h"

#include <jsoncpp/json/value.h>
#include <memory>
#include <string>

namespace wim {

bool OnlineNotifyAddFriend(ChatSession::Ptr user, const Json::Value &request);

int OfflineAddFriend(int seq, int from, int to, const std::string &msgData);

int OnlineRemoveFriend(int seq, int from, int to, ChatSession::Ptr toSession);
int OfflineRemoveFriend(int seq, int from, int to, const std::string &msgData);

void SerachUser(ChatSession::Ptr session, unsigned int msgID,
                const Json::Value &msgData);

void NotifyAddFriend(ChatSession::Ptr session, unsigned int msgID,
                     const Json::Value &msgData);

void RemoveFriend(ChatSession::Ptr session, unsigned int msgID,
                  const Json::Value &msgData);
}; // namespace wim