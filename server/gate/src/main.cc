#include "Configer.h"
#include "ImGateway.h"
#include "ImSession.h"
#include "Logger.h"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <spdlog/common.h>
#include <string>
// #include "StateRpc.h"

int main(int argc, char *argv[]) {

  if (argc < 3) {
    std::cout << "使用: " << argv[0] << " [配置文件]  [日志级别]\n";
    return -1;
  }

  auto loadSuccess = Configer::loadConfig(argv[1]);
  auto node = Configer::getNode("server");
  if (!loadSuccess || node.IsNull())
    return -1;

  std::cout << "使用: ";

  for (int i = 0; i < argc; i++)
    std::cout << argv[i] << " ";
  std::cout << std::endl;

  auto level = std::string(argv[2]);
  if (level == "--debug") {
    wim::setLoggerLevel(spdlog::level::debug);
  } else if (level == "--info") {
    wim::setLoggerLevel(spdlog::level::info);
  } else if (level == "--trace") {
    wim::setLoggerLevel(spdlog::level::trace);
  } else {
    std::cout << "Invalid log level: " << level
              << ". Valid options: --debug, --info, --trace\n";
  }
  wim::ImGateway::Ptr gateway(new wim::ImGateway(2020));
  gateway->start();
}
