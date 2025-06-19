#pragma once
#include "Const.h"
#include "DbGlobal.h"
#include "chatSession.h"
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
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

  void setSession(ChatSession::ptr session) { chat = session; }
  void setUser(db::User::Ptr user) { this->user = user; }
  bool login(bool isFirstLogin = true);
  void quit();

  bool searchUser(const std::string &username);
  void serachUserHandle(const Json::Value &response);

  bool notifyAddFriend(long uid, const std::string &requestMessage);
  bool replyAddFriend(long uid, bool accept, const std::string &replyMessage);

  void replyAddFriendSenderHandle(const Json::Value &response);
  void replyAddFriendRecvierHandle(const Json::Value &response);
  void notifyAddFriendSenderHandle(const Json::Value &response);
  void notifyAddFriendRecvierHandle(const Json::Value &response);
  void loginInitHandle(const Json::Value &response);
  void textSenderHandle(const Json::Value &response);
  void textRecvierHandle(const Json::Value &response);
  void uploadFileHandle(const Json::Value &response);

  bool ping();
  bool OnheartBeat(int count = 0);
  void pingHandle(const Json::Value &response);
  void arrhythmiaHandle(long uid);

  void initUserInfo(db::UserInfo::Ptr userInfo);
  void initUserInfoHandle(const Json::Value &response);

  bool pullFriendList();
  bool pullFriendApplyList();
  bool pullSessionMessageList(long id);
  bool pullMessageList();

  void pullFriendListHandle(const Json::Value &response);
  void pullFriendApplyListHandle(const Json::Value &response);
  void pullMessageListHandle(const Json::Value &response);

  void sendTextMessage(long uid, const std::string &message);
  void onReWrite(long id, const std::string &message, long serviceId,
                 int count = 0);

  using HandleType = std::function<void(Json::Value &response)>;
  void RegisterHandle(unsigned int msgID, HandleType handle);
  void handleRun(Tlv::Ptr protocolData);
  bool CheckAckCache(int64_t seq);
  void nullHandle(const Json::Value &response);

  db::User::Ptr user{};
  db::UserInfo::Ptr userInfo{};

  std::queue<Tlv> requestQueue{};
  ChatSession::ptr chat{};
  std::map<int, std::shared_ptr<net::steady_timer>> seqCacheExpireMap{};
  std::shared_ptr<net::steady_timer> messageReadTimer{};
  std::vector<db::Message> messageQueue{};

  std::mutex comsumeMessageMutex{};

  // handle
  std::map<int, std::function<void(Json::Value &response)>> handleMap{};
  // info
  std::map<long, db::FriendApply::Ptr> friendApplyMap{};
  std::map<long, db::UserInfo::Ptr> friendMap{};

  std::map<long, db::Message::MessageGroup> messageListMap{};

  // 群聊
  std::map<long, db::GroupMember::MemberList> groupMemberMap{};
  bool createGroup(const std::string &groupName);
  bool joinGroup(long groupId, const std::string &requestMessage);
  bool replyJoinGroup(long groupId, long replyJoinGroup, bool accept);
  bool quitGroup(long groupId);
  bool sendGroupMessage(long groupId, const std::string &message);
  bool pullGroupMember(long groupId);
  bool pullGroupMessage(long groupId, long lastMsgId, int limit);

  void createGroupHandle(Json::Value &response);

  void joinGroupSenderHandle(Json::Value &response);
  void joinGroupRecvierHandle(Json::Value &response);

  void replyJoinGroupSenderHandle(Json::Value &response);
  void replyJoinGroupRecvierHandle(Json::Value &response);

  void quitGroupHandle(Json::Value &response);
  void sendGroupTextMessageHandle(Json::Value &response);
  void pullGroupMemberHandle(Json::Value &response);
  void pullGroupMessageHandle(Json::Value &response);

  // 文件
  std::map<std::string, db::File::Ptr> fileMap{};
  bool sendFile(long toId, const std::string &filePath);
  bool uploadFile(const std::string &filePath);
  void sendFileHandle(Json::Value &response);
  void recvFileHandle(Json::Value &response);
};
}; // namespace wim