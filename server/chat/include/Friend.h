#pragma once
#include "Channel.h"
#include "ChatSession.h"

#include <memory>
#include <string>

class Friend : public std::enable_shared_from_this<Friend> {
public:
  using Ptr = std::shared_ptr<Friend>;

  Friend(int from, int to, std::string name = "", std::string icon = "");
  ~Friend();

  std::pair<int, int> getParticipants() const {
    return std::make_pair(channel->getFrom(), channel->getTo());
  }
  int getID() const { return channel->getId(); }

private:
  Channel::Ptr channel;
  std::string name;
  std::string icon;
};

static std::unordered_map<size_t, std::vector<Friend::Ptr>> friendManager;

int OnlineAddFriend(int seq, int from, int to,
                    std::shared_ptr<ChatSession> toSession);

int OfflineAddFriend(int seq, int from, int to, const std::string &msgData);

void AddFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
               const std::string &msgData);
int OnlineRemoveFriend(int seq, int from, int to,
                       std::shared_ptr<ChatSession> toSession);
int OfflineRemoveFriend(int seq, int from, int to, const std::string &msgData);
void RemoveFriend(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const std::string &msgData);