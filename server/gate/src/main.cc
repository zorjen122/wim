﻿
#include "Configer.h"
#include "GateServer.h"
#include "IocPool.h"
#include "StateClient.h"

#include "Mysql.h"
#include "Redis.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

int main() {
  try {
    auto existConfig = Configer::loadConfig("../config.yaml");
    if (!existConfig) {
      spdlog::error("Config load failed");
      return 0;
    }

    auto config = Configer::getNode("server");
    if (!config || !config["gateway"]) {
      spdlog::error("gateway config not found");
      return 0;
    }

    wim::IocPool::GetInstance();
    wim::db::MysqlDao::GetInstance();
    wim::db::RedisDao::GetInstance();
    wim::rpc::StateClient::GetInstance();

    unsigned short port = config["gateway"]["port"].as<unsigned short>();

    net::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](const boost::system::error_code &error, int signal_number) {
          if (error)
            return;

          ioc.stop();
        });

    auto gate = std::make_shared<wim::GateServer>(ioc, port);
    gate->Start();
    std::cout << "Gate Server listen on port: " << port << "\n";

    ioc.run();
  } catch (std::exception const &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
