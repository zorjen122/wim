#pragma once
#include "chatSession.h"
// #include "net.h"
#include "DbGlobal.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <condition_variable>
#include <jsoncpp/json/json.h>
#include <memory>
#include <queue>
#include <string>
#include <thread>

namespace wim {

Json::Value __parseJson(const std::string &source);

struct Gate {

  Gate(net::io_context &iocontext, const std::string &ip,
       const std::string &port);

  std::pair<Endpoint, int> signIn(const std::string &username,
                                  const std::string &password);
  bool signUp(const std::string &username, const std::string &password,
              const std::string &email);
  bool signOut();
  bool fogetPassword(const std::string &username);

  bool initUserInfo(db::UserInfo::Ptr userInfo);

  std::string __parseResponse();
  void __clearStatusMessage();

  net::io_context &context;
  beast::tcp_stream stream;
  beast::flat_buffer buffer;
  http::request<http::string_body> request;
  http::response<http::dynamic_body> response;
  std::map<std::string, db::User::Ptr> users;
  tcp::resolver::results_type endpoint;

  bool onConnected;
};

struct Message {
  enum Type { TEXT, FILE, IMAGE };
  Type type;
  long id;
  long fromUid;
  long toUid;
  std::string source;
};
#include "Const.h"
struct Chat : public Singleton<Chat> {

  using Ptr = std::shared_ptr<Chat>;
  Chat();
  ~Chat() { LOG_INFO(wim::businessLogger, "Chat::~Chat()"); }

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
  std::vector<Message> messageQueue{};

  std::mutex comsumeMessageMutex{};

  // info
  std::map<long, db::FriendApply::Ptr> friendApplyMap{};
  std::map<long, db::UserInfo::Ptr> friendMap{};

  std::map<long, db::Message::MessageGroup> messageListMap{};
};

struct Client {

  long uid;
  std::string username;
  std::string password;

  std::shared_ptr<Gate> gate;
  std::shared_ptr<Chat> chat;
};
}; // namespace wim
