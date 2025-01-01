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
#include "Singleton.h"
#include "data.h"

class ServiceSystem : public Singleton<ServiceSystem> {
  friend class Singleton<ServiceSystem>;

 public:
  using Callback_t = std::function<void(std::shared_ptr<ChatSession>,
                                        const short &, const string &)>;

  ~ServiceSystem();
  void PushLogicPackage(std::shared_ptr<protocol::LogicPackage> msg);

 private:
  void testPush(std::shared_ptr<ChatSession> session, const short &msg_id,
                const string &msg_data);

 private:
  ServiceSystem();
  void DoActive();
  void init();
  void LoginHandler(std::shared_ptr<ChatSession> session, const short &msg_id,
                    const string &msg_data);
  void SearchInfo(std::shared_ptr<ChatSession> session, const short &msg_id,
                  const string &msg_data);
  void AddFriendApply(std::shared_ptr<ChatSession> session, const short &msg_id,
                      const string &msg_data);
  void AuthFriendApply(std::shared_ptr<ChatSession> session,
                       const short &msg_id, const string &msg_data);
  void PushTextMessage(std::shared_ptr<ChatSession> session,
                       const short &msg_id, const string &msg_data);

  bool IsPureDigit(const std::string &str);
  void GetUserByUid(std::string uid_str, Json::Value &rtvalue);
  void GetUserByName(std::string name, Json::Value &rtvalue);
  bool GetBaseInfo(std::string base_key, int uid,
                   std::shared_ptr<UserInfo> &userinfo);
  bool GetFriendApplyInfo(int to_uid,
                          std::vector<std::shared_ptr<ApplyInfo>> &list);
  bool GetFriendList(int self_id,
                     std::vector<std::shared_ptr<UserInfo>> &user_list);

  std::thread _worker_thread;
  std::queue<std::shared_ptr<protocol::LogicPackage>> _messageGroup;
  std::mutex _mutex;
  std::condition_variable _consume;
  bool _isStop;
  std::map<short, Callback_t> _serviceGroup;
};
