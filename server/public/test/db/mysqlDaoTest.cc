#include "Configer.h"
#include "Mysql.h"
#include <cassert>

namespace wim::db {

class MysqlDaoTest {
public:
  static void runAllTests() {
    testUserRegistration();
    testUserInfoOperations();
    testFriendApplyOperations();
    testFriendOperations();
    testMessageOperations();
    cleanupTestData();
  }

private:
  static constexpr std::string_view TEST_USERNAME = "zorjen";
  static constexpr int TEST_UID = 2000;
  static constexpr int TEST_FRIEND_UID = 9998;
  static constexpr int TEST_MESSAGE_ID = 10000;

  static void testUserRegistration() {
    auto dao = MysqlDao::GetInstance();

    // 测试用户注册
    User::Ptr newUser(new User());
    newUser->uid = TEST_UID;
    newUser->email = "test@test.com";
    newUser->username = "test";
    newUser->password = "123456";

    int regResult = dao->userRegister(newUser);
    assert(regResult != -1 && "Registration failed");

    // 验证用户查询
    User::Ptr fetchedUser = dao->getUser(TEST_USERNAME.data());
    assert(fetchedUser != nullptr && "User not found");
    assert(fetchedUser->email == newUser->email && "Email mismatch");
  }

  static void testUserInfoOperations() {
    auto dao = MysqlDao::GetInstance();
    // 插入用户信息
    UserInfo::Ptr info(
        new UserInfo(TEST_UID, "Test User", 25, "male", "/images/test.jpg"));

    int infoResult = dao->insertUserInfo(info);
    assert(infoResult != -1 && "User info insert failed");

    // 查询验证
    UserInfo::Ptr fetchedInfo = dao->getUserInfo(TEST_UID);
    assert(fetchedInfo != nullptr && "User info not found");
    assert(fetchedInfo->name == "Test User" && "Name mismatch");

    // 更新测试
    int updateResult = dao->updateUserInfoName(TEST_UID, "Updated Name");
    assert(updateResult == 0 && "Update name failed");

    int updateAgeResult = dao->updateUserInfoAge(TEST_UID, 30);
    assert(updateAgeResult == 0 && "Update age failed");

    int updateSexResult = dao->updateUserInfoSex(TEST_UID, "female");
    assert(updateSexResult == 0 && "Update sex failed");

    int updateHeadImageResult =
        dao->updateUserInfoHeadImageURL(TEST_UID, "/images/test2.jpg");
    assert(updateHeadImageResult == 0 && "Update head image failed");
  }

  static void testFriendApplyOperations() {
    auto dao = MysqlDao::GetInstance();
    // 添加好友
    FriendApply::Ptr newFriendApply(new FriendApply(TEST_UID, TEST_FRIEND_UID));

    int addResult = dao->insertFriendApply(newFriendApply);
    assert(addResult != -1 && "Add friend failed");

    // 验证好友列表
    auto friendApply = dao->getFriendApplyList(TEST_UID);
    assert(friendApply != nullptr && "Friend list empty");
    assert(!friendApply->empty() && "Friend not found in list");

    // 删除好友测试
    int delResult = dao->deleteFriendApply(TEST_UID, TEST_FRIEND_UID);
    assert(delResult == 0 && "Delete friend failed");
  }

  static void testFriendOperations() {
    auto dao = MysqlDao::GetInstance();
    // 添加好友
    Friend::Ptr newFriend(
        new Friend(TEST_UID, TEST_FRIEND_UID, "2023-01-01 00:00:00", 1));

    int addResult = dao->insertFriend(newFriend);
    assert(addResult != -1 && "Add friend failed");

    // 验证好友列表
    auto friends = dao->getFriendList(TEST_UID);
    assert(friends != nullptr && "Friend list empty");
    assert(!friends->empty() && "Friend not found in list");

    // 删除好友测试
    int delResult = dao->deleteFriend(TEST_UID, TEST_FRIEND_UID);
    assert(delResult == 0 && "Delete friend failed");
  }

  static void testMessageOperations() {
    auto dao = MysqlDao::GetInstance();
    // 插入测试消息
    Message::Ptr msg(new Message(TEST_MESSAGE_ID, TEST_UID, TEST_FRIEND_UID,
                                 "session_123",
                                 1, // text
                                 "Hello World",
                                 1, // wait
                                 "2023-01-01 00:00:00"));

    int msgResult = dao->insertMessage(msg);
    assert(msgResult != -1 && "Message insert failed");

    // 查询消息
    auto messages =
        dao->getUserMessage(TEST_UID, TEST_FRIEND_UID, TEST_MESSAGE_ID, 10);
    assert(messages != nullptr && "No messages found");
    assert(!messages->empty() && "Message not retrieved");

    // 更新消息状态
    int updateResult =
        dao->updateMessage(TEST_MESSAGE_ID, 2); // status 2 is done
    assert(updateResult == 0 && "Message update failed");
  }

  static void cleanupTestData() {
    auto dao = MysqlDao::GetInstance();

    // 清理测试数据
    int delResult = 0;

    delResult = dao->deleteFriendApply(TEST_UID, TEST_FRIEND_UID);
    assert(delResult == 0 && "Delete friend apply failed");

    delResult = dao->deleteFriend(TEST_UID, TEST_FRIEND_UID);
    assert(delResult == 0 && "Delete friend failed");

    delResult = dao->deleteMessage(TEST_UID, TEST_MESSAGE_ID, 1);
    assert(delResult == 0 && "Delete message failed");

    delResult = dao->deleteUser(TEST_UID);
    assert(delResult == 0 && "Delete user failed");
  }
};
} // namespace wim::db

int main() {

  // Configer::loadConfig("../config.yaml");

  // wim::db::MysqlDaoTest::runAllTests();

  return 0;
}
