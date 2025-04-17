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

  auto chatHost = config["im"]["host"].as<std::string>();
  auto chatPort = config["im"]["port"].as<std::string>();

  spdlog::info("gate host: {}, port: {}", gateHost, gatePort);
  spdlog::info("chat host: {}, port: {}", chatHost, chatPort);

  net::io_context ioContext;
  IM::Gate gate(ioContext, gateHost, gatePort);

  gate.signUp("zorjen", "123456", "1001@qq.com");
  auto endpoint = gate.signIn("1001@qq.com", "123456");
  spdlog::info("chat endpoint: {}, {}", endpoint.ip, endpoint.port);

  ioContext.run();
  return 0;
}