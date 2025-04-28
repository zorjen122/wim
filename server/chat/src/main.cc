
#include "ImActiver.h"
#include "Logger.h"
#include <boost/asio/signal_set.hpp>
#include <spdlog/common.h>
#include <string>

int main(int argc, char *argv[]) {

  if (argc < 3) {
    std::cout << "Usage: " << argv[0]
              << " [config file] [--normal or --backup] | other: [logger level]"
              << std::endl;
    return -1;
  }

  auto loadSuccess = Configer::loadConfig(argv[1]);
  auto node = Configer::getNode("server");
  if (!loadSuccess || node.IsNull())
    return -1;

  if (argc == 4) {
    auto level = std::string(argv[3]);
    if (level == "--debug") {
      wim::setLoggerLevel(spdlog::level::debug);
    } else if (level == "--info") {
      wim::setLoggerLevel(spdlog::level::info);
    } else if (level == "--trace") {
      wim::setLoggerLevel(spdlog::level::trace);
    }
  }

  if (argc == 3 && std::string(argv[2]) == "--normal") {
    LOG_INFO(wim::businessLogger, "ImServiceRunner 启动, 模式: normal");
    wim::ImServiceRunner::GetInstance()->Activate(
        wim::ImServiceRunner::ModeType::NORMAL_ACTIVE);
  }

  return 0;
}
