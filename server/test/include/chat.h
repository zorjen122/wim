#pragma once
#include "Const.h"
#include "DbGlobal.h"
#include "TcpMessageCodec.h"
#include "chatSession.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace wim {
struct Chat : Singleton<Chat> {
  using Ptr = std::shared_ptr<Chat>;
  Chat();
  ~Chat() {
    LOG_INFO(wim::businessLogger, "Chat::~Chat() | session use count: {}",
             chat.use_count());
  }

  void setSession(ChatSession::Ptr session) {
    chat = session;
  }
  void setUser(db::User::Ptr user) {
    this->user = user;
  }
  void setAuthToken(std::string token) {
    authToken = std::move(token);
  }
  bool login(bool isFirstLogin = true);
  bool waitLoginReady(int timeoutSeconds = 5);
  void quit();

  bool searchUser(const std::string &username);
  void serachUserHandle(const TcpPacket &response);

  bool notifyAddFriend(long uid, const std::string &requestMessage);
  bool replyAddFriend(long uid, bool accept, const std::string &replyMessage);

  void replyAddFriendSenderHandle(const TcpPacket &response);
  void replyAddFriendRecvierHandle(const TcpPacket &response);
  void notifyAddFriendSenderHandle(const TcpPacket &response);
  void notifyAddFriendRecvierHandle(const TcpPacket &response);
  void loginInitHandle(const TcpPacket &response);
  void textSenderHandle(const TcpPacket &response);
  void textRecvierHandle(const TcpPacket &response);
  void uploadFileHandle(const TcpPacket &response);

  bool ping();
  bool OnheartBeat(int count = 0);
  void pingHandle(const TcpPacket &response);
  void arrhythmiaHandle(long uid);

  void initUserInfo(db::UserInfo::Ptr userInfo);
  void initUserInfoHandle(const TcpPacket &response);

  bool pullFriendList();
  bool pullFriendApplyList();
  bool pullSessionMessageList(long id);
  bool pullMessageList();

  void pullFriendListHandle(const TcpPacket &response);
  void pullFriendApplyListHandle(const TcpPacket &response);
  void pullMessageListHandle(const TcpPacket &response);

  void sendTextMessage(long uid, const std::string &message);
  void onReWrite(long id, const std::string &message, long serviceId,
                 int count = 0);

  using HandleType = std::function<void(TcpPacket &response)>;
  void RegisterHandle(unsigned int msgID, HandleType handle);
  void handleRun(Tlv::Ptr protocolData);
  bool CheckAckCache(int64_t seq);
  void nullHandle(const TcpPacket &response);

  db::User::Ptr user{};
  db::UserInfo::Ptr userInfo{};
  std::string authToken{};

  std::queue<Tlv> requestQueue{};
  ChatSession::Ptr chat{};
  std::map<int, std::shared_ptr<net::steady_timer>> seqCacheExpireMap{};
  std::shared_ptr<net::steady_timer> messageReadTimer{};
  std::vector<db::Message> messageQueue{};

  std::mutex comsumeMessageMutex{};
  std::mutex loginMutex{};
  std::condition_variable loginCv{};
  bool loginInitDone{false};
  bool loginInitOk{false};

  // handle
  std::map<int, std::function<void(TcpPacket &response)>> handleMap{};
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

  void createGroupHandle(TcpPacket &response);

  void joinGroupSenderHandle(TcpPacket &response);
  void joinGroupRecvierHandle(TcpPacket &response);

  void replyJoinGroupSenderHandle(TcpPacket &response);
  void replyJoinGroupRecvierHandle(TcpPacket &response);

  void quitGroupHandle(TcpPacket &response);
  void sendGroupTextMessageHandle(TcpPacket &response);
  void pullGroupMemberHandle(TcpPacket &response);
  void pullGroupMessageHandle(TcpPacket &response);

  // 文件
  std::map<std::string, db::File::Ptr> fileMap{};
  bool sendFile(long toId, const std::string &filePath);
  bool uploadFile(const std::string &filePath);
  void sendFileHandle(TcpPacket &response);
  void recvFileHandle(TcpPacket &response);
};
};  // namespace wim
