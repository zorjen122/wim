#pragma once
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

#include <functional>
#include <map>
#include <queue>
#include <thread>
#include <unordered_map>

#include "ChatSession.h"
#include "Const.h"
#include "Package.h"

class ServiceSystem : public Singleton<ServiceSystem> {
  friend class Singleton<ServiceSystem>;

public:
  using HandleType = std::function<void(std::shared_ptr<ChatSession>,
                                        unsigned short, const string &)>;

  ~ServiceSystem();
  void PushService(std::shared_ptr<protocol::LogicPackage> package);

private:
  ServiceSystem();
  void run();
  void Init();
  void WeachKeepAlive();

  void PingKeepAlive(std::shared_ptr<ChatSession> session, unsigned short msgID,
                     const string &msgData);
  void LoginHandler(std::shared_ptr<ChatSession> session, unsigned short msgID,
                    const string &msgData);

  void OnlinePullHandler(std::shared_ptr<ChatSession> session,
                         unsigned short msgID, const string &msgData);

  void SearchUser(std::shared_ptr<ChatSession> session, unsigned short msgID,
                  const string &msg_data);
  void AddFriendApply(std::shared_ptr<ChatSession> session,
                      unsigned short msgID, const string &msg_data);
  void AuthFriendApply(std::shared_ptr<ChatSession> session,
                       unsigned short msg_id, const string &msgData);
  void PushTextMessage(std::shared_ptr<ChatSession> session,
                       unsigned short msg_id, const string &msgData);

  bool IsPureDigit(const std::string &str);
  void GetUserByUid(std::string uid_str, Json::Value &rtvalue);
  void GetUserByName(std::string name, Json::Value &rtvalue);
  bool GetFriendApplyInfo(int toID,
                          std::vector<std::shared_ptr<ApplyInfo>> &list);
  bool GetFriendList(int self_id,
                     std::vector<std::shared_ptr<UserInfo>> &user_list);

  std::thread worker;
  std::queue<std::shared_ptr<protocol::LogicPackage>> _messageGroup;
  std::mutex _mutex;
  std::condition_variable _consume;
  bool isStop;
  std::map<short, HandleType> _serviceGroup;
};
