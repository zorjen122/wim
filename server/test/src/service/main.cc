#include "Configer.h"
#include "chat.h"
#include "gate.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc < 6) {
    spdlog::info("Usage: [username] [password] [userId] [chatIp] [chatPort]");
    return -1;
  }

  const char *configPath = std::getenv("WIM_CONFIG");
  bool loadSuccess = Configer::loadConfig(
      configPath ? configPath : "../conf/test-client.yaml");
  YAML::Node node = Configer::getNode("server");
  if (!loadSuccess || node.IsNull())
  {
    spdlog::info("This is expected, currently load config is not supported");
  }

  net::io_context ioContext;
  auto work = net::make_work_guard(ioContext);

  // auto gateHost = config["gateway"]["host"].as<std::string>();
  // auto gatePort = config["gateway"]["port"].as<std::string>();
  // spdlog::info("gate host: {}, port: {}", gateHost, gatePort);
  // wim::Gate gate(ioContext, gateHost, gatePort);

  // // gate.signUp("test", "123456", "1001@qq.com");
  // spdlog::info("sign up...");
  // std::pair<wim::Endpoint, int> result;
  // if (argc >= 3) {
  //   result = gate.signIn(argv[1], argv[2]);
  // }

  // auto endpoint = result.first;
  // auto init = result.second;
  // spdlog::info("chat endpoint: {}, {}", endpoint.ip, endpoint.port);

  // wim::UserInfo::Ptr userinfo;
  // auto user = gate.users["test"];

  // if (init == 1) {
  //   userinfo.reset(new wim::UserInfo(user->uid, "Peter", 25, "male",
  //                                   "http://example.com/head.jpg"));
  // }

  int status = 0;

  bool init = false;
  std::string username = argv[1];
  std::string password = argv[2];
  long uid = atol(argv[3]);
  std::string chatIp = argv[4];
  std::string chatPort = argv[5];
  Endpoint endpoint(chatIp, chatPort);

  auto user = std::make_shared<wim::db::User>();
  user->uid = uid;
  user->username = username;
  user->password = password;
  auto chat = wim::Chat::GetInstance();
  chat->setUser(user);
  LOG_INFO(wim::businessLogger, "uid: {}, username: {}, password: {}",
           user->uid, user->username, user->password);

  wim::ChatSession::Ptr session = nullptr;
  std::thread t([&]() {
    LOG_INFO(wim::netLogger, "iocontext.run() started!");
    ioContext.run();
    LOG_INFO(wim::netLogger, "iocontext.run() exited!");
  });

  auto clearUp = [&]() {
    if (session)
      session->Close();
    work.reset();
    ioContext.stop();
    if (t.joinable() && std::this_thread::get_id() != t.get_id())
      t.join();
    session.reset();
    if (chat)
      chat->setSession(nullptr);
  };

  std::cout << "\n【请求方式：【命令】 (q/Q 退出)】\n";

  try {
    session.reset(new wim::ChatSession(ioContext, endpoint));
    chat->setSession(session->GetSharedSelf());

    if (!session->isConnected())
      throw std::runtime_error("session connect failed");
    session->Start();
    status = chat->login(init);
    if (status == false) {
      LOG_ERROR(wim::netLogger, "login failed");
      return -1;
    }
    if (!chat->waitLoginReady()) {
      LOG_ERROR(wim::netLogger, "login init response timed out");
      return -1;
    }

    // chat->OnheartBeat();

    net::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code &ec, int sig) {
      if (chat)
        chat->quit();
      clearUp();
    });

    while (true) {
      std::string command;
      if (!(std::cin >> command)) {
        chat->quit();
        break;
      }

      if (command == "q" || command == "Q") {
        std::cout << "Exiting..." << std::endl;
        chat->quit();
        break;
      } else if (command == "addFriend") {
        // 执行添加好友操作
        long userId = -1;
        std::cout << "申请好友ID为: ";
        std::cin >> userId;
        if (userId <= 0) {
          std::cerr << "无效的好友ID\n";
          continue;
        }
        status = chat->notifyAddFriend(userId, "Hello!");
        if (status == 0) {
          std::cerr << "网络异常，请稍后重试..." << std::endl;
        }
      } else if (command == "showFriendApplyList") {
        std::cout << "好友请求列表: \n";
        for (auto [key, it] : chat->friendApplyMap) {
          std::cout << "[用户ID: " << key << ", 消息: " << it->content << "]\n";
        }
      } else if (command == "replyAddFriend") {
        bool accept = false;  // 同意 == true, 拒绝 == false
        std::string uid, replyMessage;

        std::cout << "请输入回复消息的用户ID: ";
        std::cin >> uid;
        std::cout << "请输入回复消息: ";
        std::cin >> replyMessage;
        std::cout << "是否同意? (y/n): ";
        std::string tmp;
        std::cin >> tmp;
        if (tmp == "y" || tmp == "Y") {
          accept = true;
        } else if (tmp == "n" || tmp == "N") {
          accept = false;
        } else {
          std::cerr << "无效的输入，请输入y（同意）或n（拒绝）" << std::endl;
          continue;
        }
        chat->replyAddFriend(std::atol(uid.c_str()), accept, replyMessage);

      } else if (command == "textSend") {
        std::string toUid, message;
        std::cout << "请输入接收消息的用户ID: ";
        std::cin >> toUid;
        std::cout << "请输入消息内容: ";
        std::cin >> message;
        chat->sendTextMessage(std::atol(toUid.c_str()), message);
      } else if (command == "pullSessionMessage") {
        std::string toUid;
        std::cout << "请输入拉取的用户ID消息: ";
        std::cin >> toUid;
        chat->pullSessionMessageList(std::atol(toUid.c_str()));
      } else if (command == "pullMessage") {
        chat->pullMessageList();
      } else if (command == "pullFriendList") {
        chat->pullFriendList();
      } else if (command == "pullFriendApplyList") {
        chat->pullFriendApplyList();
      } else if (command == "ping") {
        chat->ping();
      } else if (command == "searchUser") {
        std::string username;
        std::cout << "请输入用户名: ";
        std::cin >> username;
        chat->searchUser(username);
      } else if (command == "uploadFile") {
        std::string fileName;
        std::cout << "请输入文件全称: ";
        std::cin >> fileName;
        chat->uploadFile(fileName);
      } else if (command == "createGroup") {
        std::string groupName;
        std::cout << "请输入群组名称: ";
        std::cin >> groupName;
        chat->createGroup(groupName);
      } else if (command == "joinGroup") {
        std::string groupId;
        std::cout << "请输入群组ID: ";
        std::cin >> groupId;
        std::string requestMessage;
        std::cout << "请输入申请消息: ";
        std::cin >> requestMessage;
        chat->joinGroup(std::atol(groupId.c_str()), requestMessage);
      } else if (command == "replyJoinGroup") {
        std::string groupId, requestorUid;
        bool accept = false;  // 同意 == true, 拒绝 == false
        std::cout << "请输入群组ID: ";
        std::cin >> groupId;
        std::cout << "请输入申请者ID: ";
        std::cin >> requestorUid;
        std::cout << "是否同意? (y/n): ";
        std::string tmp;
        std::cin >> tmp;
        if (tmp == "y" || tmp == "Y") {
          accept = true;
        } else if (tmp == "n" || tmp == "N") {
          accept = false;
        } else {
          std::cerr << "无效的输入，请输入y（同意）或n（拒绝）" << std::endl;
          continue;
        }
        chat->replyJoinGroup(std::atol(groupId.c_str()),
                             std::atol(requestorUid.c_str()), accept);
      } else if (command == "sendGroupMessage") {
        std::string groupId, message;
        std::cout << "请输入群组ID: ";
        std::cin >> groupId;
        std::cout << "请输入消息内容: ";
        std::cin >> message;
        chat->sendGroupMessage(std::atol(groupId.c_str()), message);
      } else if (command == "pullGroupMember") {
        std::string groupId;
        std::cout << "请输入群组ID: ";
        std::cin >> groupId;
        chat->pullGroupMember(std::atol(groupId.c_str()));
      } else if (command == "quitGroup") {
        std::string groupId;
        std::cout << "请输入群组ID: ";
        std::cin >> groupId;
        chat->quitGroup(std::atol(groupId.c_str()));
      } else if (command == "sendFile") {
        std::string toUid, fileName;
        std::cout << "请输入接收消息的用户ID: ";
        std::cin >> toUid;
        std::cout << "请输入文件全称: ";
        std::cin >> fileName;
        chat->sendFile(std::atol(toUid.c_str()), fileName);
      } else {
        std::cout << "无效命令" << std::endl;
      }
    }
    clearUp();
  } catch (std::exception &e) {
    spdlog::error("异常: {}", e.what());
    clearUp();
  }

  return 0;
}
