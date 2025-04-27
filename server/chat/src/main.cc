#include "RpcService.h"

#include "ImActiver.h"
#include "Logger.h"
#include <boost/asio/signal_set.hpp>
#include <string>

int main(int argc, char *argv[]) {

  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " [config file] [--normal or --backup]"
              << std::endl;
    return -1;
  }

  auto loadSuccess = Configer::loadConfig(argv[1]);
  auto node = Configer::getNode("server");
  if (!loadSuccess || node.IsNull())
    return -1;

  if (argc == 3 && std::string(argv[2]) == "--normal") {
    LOG_INFO(wim::businessLogger, "ImServiceRunner started, mode: normal");
    wim::ImServiceRunner::GetInstance()->Activate(
        wim::ImServiceRunner::RunnerType::NORMAL_ACTIVE);
  }

  return 0;
}
