#include "Configer.h"
#include "GatewayServer.h"
#include "Logger.h"
#include "MessageLink.h"
#include "Mysql.h"
#include "Redis.h"
#include "SessionRegistry.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <vector>

int main(int argc, char **argv) {
  const char *configPath = std::getenv("WIM_CONFIG");
  if (!configPath && argc > 1)
    configPath = argv[1];
  if (!Configer::loadConfig(configPath ? configPath
                                       : "../conf/gateway-hunan.yaml")) {
    spdlog::error("Connection Gateway config load failed");
    return EXIT_FAILURE;
  }

  auto config = Configer::getNode("server");
  if (!config || !config["self"] || !config["self"]["name"] ||
      !config["self"]["port"]) {
    spdlog::error("Connection Gateway self config is missing");
    return EXIT_FAILURE;
  }

  const std::string gatewayId = config["self"]["name"].as<std::string>();
  const unsigned short port = config["self"]["port"].as<unsigned short>();
  const std::string instanceId =
      boost::uuids::to_string(boost::uuids::random_generator{}());

  wim::db::MysqlDao::GetInstance();
  wim::db::RedisDao::GetInstance();

  boost::asio::io_context ioContext;
  boost::asio::thread_pool businessPool(4);
  wim::connection::SessionRegistry registry(gatewayId, instanceId);
  wim::connection::MessageLinkManager messageLinks(ioContext, businessPool,
                                                   gatewayId, instanceId);
  messageLinks.SetDeliveryHandler(
      [&registry](const wim::gateway::DeliveryEnvelope &delivery) {
        return registry.Deliver(delivery);
      });
  messageLinks.Start();

  wim::connection::GatewayServer server(ioContext, port, registry, messageLinks,
                                        businessPool);
  boost::asio::co_spawn(ioContext, server.Run(), boost::asio::detached);

  boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code &error, int) {
    if (error)
      return;
    messageLinks.Stop();
    ioContext.stop();
  });

  const auto workerCount =
      std::clamp<std::size_t>(std::thread::hardware_concurrency(), 2, 8);
  std::vector<std::thread> workers;
  workers.reserve(workerCount - 1);
  for (std::size_t i = 1; i < workerCount; ++i)
    workers.emplace_back([&]() { ioContext.run(); });
  LOG_INFO(wim::businessLogger,
           "Connection Gateway started, id: {}, instance: {}, port: {}",
           gatewayId, instanceId, port);
  ioContext.run();
  for (auto &worker : workers)
    worker.join();

  messageLinks.Stop();
  businessPool.stop();
  businessPool.join();
  wim::db::MysqlDao::GetInstance()->Close();
  wim::db::RedisDao::GetInstance()->Close();
  return EXIT_SUCCESS;
}
