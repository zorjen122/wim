#pragma once

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <mysql-cppconn-8/mysqlx/devapi/common.h>
#include <mysql-cppconn-8/mysqlx/devapi/result.h>
#include <mysql-cppconn-8/mysqlx/xdevapi.h>

#include "Const.h"
#include "DbGlobal.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

namespace wim::db {

struct SqlConnection {
  SqlConnection(mysqlx::Session *connection, int64_t lastTime);
  ~SqlConnection();

  std::unique_ptr<mysqlx::Session> session;
  int64_t leaseTime;
};

class MysqlPool {
public:
  using Ptr = std::shared_ptr<MysqlPool>;

  MysqlPool(const std::string &host, unsigned short port,
            const std::string &user, const std::string &password,
            const std::string &schema, std::size_t maxSize = 2);

  std::unique_ptr<SqlConnection> GetConnection();
  std::unique_ptr<SqlConnection>
  ReturnConnection(std::unique_ptr<SqlConnection> con);
  bool Empty();
  void Close();
  std::size_t Size();
  ~MysqlPool();

private:
  void keepConnectionHandle();

  std::string host;
  unsigned short port;
  std::string user;
  std::string password;
  std::string schema;

  std::queue<std::unique_ptr<SqlConnection>> pool{};
  std::mutex sqlMutex{};
  std::condition_variable condVar{};
  std::atomic<bool> stopEnable{};
  std::thread keepThread{};
};

class MysqlDao : public Singleton<MysqlDao>,
                 public std::enable_shared_from_this<MysqlDao> {
private:
  MysqlPool::Ptr mysqlPool;
  friend class TestDb;

public:
  using Ptr = std::shared_ptr<MysqlDao>;
  MysqlDao();
  ~MysqlDao();

  template <typename Func>
  auto executeTemplate(Func &&processor) -> decltype(
      processor(std::declval<std::unique_ptr<mysqlx::Session> &>()));

  template <typename T> static auto defaultForType();

  void HandleError(mysqlx::Error &e);
  void Close();

  User::Ptr getUser(const std::string &username);
  UserInfo::Ptr getUserInfo(long uid);
  bool hasUserInfo(long uid);
  Friend::FriendGroup getFriendList(long uid);
  int userModifyPassword(User::Ptr user);
  int userRegister(User::Ptr user);
  int insertUserInfo(UserInfo::Ptr userInfo);
  int insertFriendApply(FriendApply::Ptr friendData);
  int updateFriendApplyStatus(FriendApply::Ptr friendData);
  int hasFriend(long uidA, long uidB);
  int insertFriend(Friend::Ptr friendData);
  int insertMessage(Message::Ptr message);
  int updateUserInfoName(long uid, const std::string &name);
  int updateUserInfoAge(long uid, int age);
  int updateUserInfoSex(long uid, const std::string &sex);
  int updateUserInfoHeadImageURL(long uid, const std::string &headImageURL);
  int updateMessage(long messageId, short status);
  FriendApply::FriendApplyGroup getFriendApplyList(long from);
  Message::MessageGroup getSessionMessage(long from, long to, int lastMessageId,
                                          int pullCount);
  Message::MessageGroup getUserMessage(long uid, int lastMessageId,
                                       int pullCount);
  int deleteUser(long uid);
  int deleteFriendApply(long fromUid, long toUid);
  int deleteFriend(long uidA, long uidB);
  int deleteMessage(long uid, int lastMessageId, int delCount);
  int hasEmail(std::string email);
  int hasUsername(std::string username);
  int insertGroup(GroupManager::Ptr group);
  int insertGroupMember(GroupMember::Ptr member);
  GroupMember::MemberList getGroupList(long uid);
  int hasGroup(long gid);
  GroupMember::MemberList getGroupRoleMemberList(long gid, short role);
  GroupMember::MemberList getGroupMemberList(long gid);
  int insertGroupApply(GroupApply::Ptr apply);
  GroupApply::GroupApplyList selectGroupApply();
  int hasGroupApply(long requestor, long gid);
  int deleteGroupApply(long requestor, long gid);
  int updateGroupApply(GroupApply::Ptr apply);
  GroupApply::GroupApplyList pullGroupApply(long requestor);
};

} // namespace wim::db