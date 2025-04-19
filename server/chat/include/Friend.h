#pragma once
#include "Channel.h"
#include "ChatSession.h"

#include "json/value.h"
#include <memory>
#include <string>

namespace wim {

int OnlineNotifyAddFriend(std::shared_ptr<ChatSession> user,
                          const Json::Value &request);

int OfflineAddFriend(int seq, int from, int to, const std::string &msgData);

int OnlineRemoveFriend(int seq, int from, int to,
                       std::shared_ptr<ChatSession> toSession);
int OfflineRemoveFriend(int seq, int from, int to, const std::string &msgData);

void SerachUser(std::shared_ptr<ChatSession> session, unsigned int msgID,
                const Json::Value &msgData);

void NotifyAddFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                     const Json::Value &msgData);

void RemoveFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const Json::Value &msgData);
}; // namespace wim