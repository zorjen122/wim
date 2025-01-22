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
                                        unsigned int, const std::string &)>;

  ~ServiceSystem();
  void PushService(std::shared_ptr<protocol::LogicPackage> package);

private:
  ServiceSystem();
  void Run();
  void Init();
  void WeachKeepAlive();

  void PingKeepAlive(std::shared_ptr<ChatSession> session, unsigned int msgID,
                     const std::string &msgData);
  void LoginHandler(std::shared_ptr<ChatSession> session, unsigned int msgID,
                    const std::string &msgData);

  void OnlinePullHandler(std::shared_ptr<ChatSession> session,
                         unsigned int msgID, const std::string &msgData);

  void SearchUser(std::shared_ptr<ChatSession> session, unsigned int msgID,
                  const std::string &msg_data);
  void AddFriendApply(std::shared_ptr<ChatSession> session, unsigned int msgID,
                      const std::string &msg_data);
  void AuthFriendApply(std::shared_ptr<ChatSession> session,
                       unsigned int msg_id, const std::string &msgData);
  void PushTextMessage(std::shared_ptr<ChatSession> session,
                       unsigned int msg_id, const std::string &msgData);

  bool IsPureDigit(const std::string &str);
  void GetUserByUid(std::string uid_str, Json::Value &rtvalue);
  void GetUserByName(std::string name, Json::Value &rtvalue);
  bool GetFriendApplyInfo(unsigned int toID,
                          std::vector<std::shared_ptr<ApplyInfo>> &list);
  bool GetFriendList(unsigned int self_id,
                     std::vector<std::shared_ptr<UserInfo>> &user_list);

  std::thread worker;
  std::queue<std::shared_ptr<protocol::LogicPackage>> _messageGroup;
  std::mutex _mutex;
  std::condition_variable _consume;
  bool isStop;
  std::map<unsigned int, HandleType> _serviceGroup;
};
