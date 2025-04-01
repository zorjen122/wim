#include "Configer.h"
#include "GateServer.h"
#include "IocPool.h"
#include "RedisOperator.h"
#include <iostream>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

int main() {
  try {
    RedisOperator::GetInstance();
    IocPool::GetInstance();

    auto existConfig = Configer::loadConfig("../config.yaml");
    if (!existConfig) {
      spdlog::error("Config load failed");
      return 0;
    }

    auto config = Configer::getConfig("server");
    if (!config || !config["self"]) {
      spdlog::error("self config not found");
      return 0;
    }

    unsigned short port = config["GateServer"]["Port"].as<unsigned short>();

    net::io_context ioc{1};
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](const boost::system::error_code &error, int signal_number) {
          if (error)
            return;

          ioc.stop();
        });

    auto gate = std::make_shared<GateServer>(ioc, port);
    gate->Start();
    std::cout << "Gate Server listen on port: " << port << "\n";

    ioc.run();
    RedisOperator::GetInstance()->Close();
  } catch (std::exception const &e) {
    std::cerr << "Error: " << e.what() << "\n";
    RedisOperator::GetInstance()->Close();
    return EXIT_FAILURE;
  }
}
