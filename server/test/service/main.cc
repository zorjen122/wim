#include "Configer.h"
#include "Const.h"
#include "Logger.h"
#include "client.h"
#include "global.h"
#include "service/chatSession.h"
#include "spdlog/spdlog.h"
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ratio>
#include <thread>
#include <yaml-cpp/node/node.h>

int main(int argc, char *argv[]) {

  if (argc < 3) {
    spdlog::error(
        "Usage: [username] [password] | [userId] [chatIp] [chatPort]");
    return -1;
  }

  bool loadSuccess = Configer::loadConfig("../config.yaml");
  YAML::Node node = Configer::getNode("server");
  if (!loadSuccess || node.IsNull())
    return -1;

  net::io_context ioContext;
  net::io_context::work work(ioContext);

  // auto gateHost = config["gateway"]["host"].as<std::string>();
  // auto gatePort = config["gateway"]["port"].as<std::string>();
  // spdlog::info("gate host: {}, port: {}", gateHost, gatePort);
  // wim::Gate gate(ioContext, gateHost, gatePort);

  // // gate.signUp("zorjen", "123456", "1001@qq.com");
  // spdlog::info("sign up...");
  // std::pair<wim::Endpoint, int> result;
  // if (argc >= 3) {
  //   result = gate.signIn(argv[1], argv[2]);
  // }

  // auto endpoint = result.first;
  // auto init = result.second;
  // spdlog::info("chat endpoint: {}, {}", endpoint.ip, endpoint.port);

  // wim::UserInfo::Ptr userinfo;
  // auto user = gate.users["zorjen"];

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
    work.get_io_context().stop();
    if (t.joinable())
      t.join();
  };

  try {
    session.reset(new wim::ChatSession(ioContext, endpoint));
    chat->setSession(session->GetSharedSelf());

    session->Start();
    status = chat->login(init);
    if (status == false) {
      LOG_ERROR(wim::netLogger, "login failed");
      return -1;
    }

    chat->ping(user->uid);
    net::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait(
        [&](const boost::system::error_code &ec, int sig) { clearUp(); });

    while (true) {
      std::cout << "Enter a command (q or Q to quit):";
      std::string command;
      std::cin >> command;

      if (command == "q" || command == "Q") {
        std::cout << "Exiting..." << std::endl;
        break;
      } else if (command == "addFriend") {
        // 执行添加好友操作
        long userId = -1;
        std::cout << "Enter the user id of the friend you want to add: ";
        std::cin >> userId;
        if (userId == -1) {
          std::cerr << "Failed to add friend. Invalid user id."
                    << "\n";
          continue;
        }
        status = chat->notifyAddFriend(userId, "Hello!");
        if (status == 0) {
          std::cerr << "Failed to add friend. Retrying..." << std::endl;
        } else {
          std::cout << "Friend added successfully!" << std::endl;
        }
      } else if (command == "showFriendApplyList") {
        std::cout << "好友请求列表: \n";
        for (auto it : chat->friendApplyList) {
          std::cout << "[用户ID: " << it->fromUid
                    << ", 消息: " << it->replyMessage << "]\n";
        }
      } else if (command == "replyAddFriend") {

        bool accept = false; // 同意 == true, 拒绝 == false
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
          std::cerr << "Invalid input. Please enter 'y' or 'n'." << std::endl;
          continue;
        }
        chat->replyAddFriend(std::atol(uid.c_str()), accept, replyMessage);

      } else if (command == "textSend") {
        std::string fromUid, toUid, message;
        std::cout << "请输入发送消息的用户ID: ";
        std::cin >> fromUid;
        std::cout << "请输入接收消息的用户ID: ";
        std::cin >> toUid;
        std::cout << "请输入消息内容: ";
        std::cin >> message;
        chat->sendMessage(std::atol(toUid.c_str()), message);
      } else {
        std::cout << "Unknown command. enter 'q' to quit." << std::endl;
      }
    }
    clearUp();
  } catch (std::exception &e) {
    spdlog::error("exception: {}", e.what());
    clearUp();
    status = -1;
  }

  return 0;
}