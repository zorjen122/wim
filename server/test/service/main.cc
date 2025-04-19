#include "Configer.h"
#include "client.h"
#include <boost/asio/io_context.hpp>
#include <yaml-cpp/node/node.h>

int main() {

  auto existConfig = Configer::loadConfig("../config.yaml");
  if (!existConfig) {
    spdlog::error("Config load failed");
    return 0;
  }

  auto config = Configer::getConfig("server");
  if (!config) {
    spdlog::error("config not found");
    return 0;
  }

  auto gateHost = config["gateway"]["host"].as<std::string>();
  auto gatePort = config["gateway"]["port"].as<std::string>();

  spdlog::info("gate host: {}, port: {}", gateHost, gatePort);

  net::io_context ioContext;
  IM::Gate gate(ioContext, gateHost, gatePort);

  gate.signUp("zorjen", "123456", "1001@qq.com");
  auto result = gate.signIn("zorjen", "123456");
  auto endpoint = result.first;
  auto init = result.second;
  // 静态测试
  // auto endpoint = IM::Endpoint("127.0.0.1", "8090");
  // auto init = true;
  spdlog::info("chat endpoint: {}, {}", endpoint.ip, endpoint.port);

  IM::UserInfo::Ptr userinfo;

  if (init == 1) {
    userinfo.reset(new IM::UserInfo(1001, "Peter", 25, "male",
                                    "http://example.com/head.jpg"));
  }

  auto user = gate.users["zorjen"];
  IM::Chat chat(ioContext, endpoint, user, userinfo);
  chat.login(init);
  // long uid = chat.searchUser("Peter");
  chat.addFriend(303818368832507905, "Hello!");
  // chat.sendMessage(uid, "hello, world!");
  chat.run();

  ioContext.run();
  return 0;
}