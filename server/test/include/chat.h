#pragma once
#include "Const.h"
#include "DbGlobal.h"
#include "chatSession.h"
#include <jsoncpp/json/json.h>
#include <memory>
#include <queue>

namespace wim {
struct Chat : Singleton<Chat> {
  using Ptr = std::shared_ptr<Chat>;
  Chat();
  ~Chat() {
    LOG_INFO(wim::businessLogger, "Chat::~Chat() | session use count: {}",
             chat.use_count());
  }

  void setSession(ChatSession::Ptr session) { chat = session; }
  void setUser(db::User::Ptr user) { this->user = user; }
  bool login(bool isFirstLogin = true);

  bool searchUser(const std::string &username);
  void serachUserHandle(const Json::Value &response);

  bool notifyAddFriend(long uid, const std::string &requestMessage);
  bool replyAddFriend(long uid, bool accept, const std::string &replyMessage);

  bool ping();
  bool OnheartBeat(int count = 0);
  void pingHandle(const Json::Value &response);
  void arrhythmiaHandle(long uid);

  bool pullFriendList();
  bool pullFriendApplyList();
  bool pullMessageList(long id);

  bool pullFriendListHandle(const Json::Value &response);
  bool pullFriendApplyListHandle(const Json::Value &response);
  bool pullMessageListHandle(const Json::Value &response);

  void sendTextMessage(long uid, const std::string &message);
  void onReWrite(long id, const std::string &message, long serviceId,
                 int count = 0);

  void handleRun(Tlv::Ptr protocolData);

  db::User::Ptr user{};
  db::UserInfo::Ptr userInfo{};

  std::queue<Tlv> requestQueue{};
  ChatSession::Ptr chat{};
  std::map<int, std::shared_ptr<net::steady_timer>> seqCacheExpireMap{};
  std::shared_ptr<net::steady_timer> messageReadTimer{};
  std::vector<db::Message> messageQueue{};

  std::mutex comsumeMessageMutex{};

  // info
  std::map<long, db::FriendApply::Ptr> friendApplyMap{};
  std::map<long, db::UserInfo::Ptr> friendMap{};

  std::map<long, db::Message::MessageGroup> messageListMap{};

  // 群聊
  std::map<long, db::GroupMember::MemberList> groupMemberMap{};
  bool createGroup(const std::string &groupName);
  bool joinGroup(long groupId);
  bool quitGroup(long groupId);
  bool sendGroupMessage(long groupId, const std::string &message);
  bool pullGroupMember(long groupId);
  bool pullGroupMessage(long groupId, long lastMsgId, int limit);

  bool createGroupHandle(Json::Value &response);
  bool joinGroupHandle(Json::Value &response);
  bool applyJoinGroupHandle(Json::Value &response);
  bool quitGroupHandle(Json::Value &response);
  bool sendGroupTextMessageHandle(Json::Value &response);
  bool pullGroupMemberHandle(Json::Value &response);
  bool pullGroupMessageHandle(Json::Value &response);

  // 文件
  std::map<std::string, db::File::Ptr> fileMap{};
  bool sendFile(long toId, const std::string &filePath);
  bool uploadFile(const std::string &filePath);
  bool sendFileHandle(Json::Value &response);
  bool recvFileHandle(Json::Value &response);
};
}; // namespace wim