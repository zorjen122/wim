#include <iostream>
#include <stdexcept>
#include <string>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

int foo() {
  try {
    // 加载配置文件
    YAML::Node config = YAML::LoadFile("config.yaml");

    // 检查 "server" 节点是否存在
    if (!config["server"]) {
      throw std::runtime_error("config.yaml: 'server' node not found");
    }

    // 检查 "Gateway" 节点是否存在
    YAML::Node gateway = config["server"]["Gateway"];
    if (!gateway) {
      throw std::runtime_error("config.yaml: 'Gateway' node not found");
    }

    // 检查 "Host" 键是否存在
    if (!gateway["Host"]) {
      throw std::runtime_error(
          "config.yaml: 'Host' key not found in 'Gateway' node");
    }

    // 检查 "Port" 键是否存在
    if (!gateway["Port"]) {
      throw std::runtime_error(
          "config.yaml: 'Port' key not found in 'Gateway' node");
    }

    // 输出 Gateway 的 Host 和 Port
    std::cout << "Gateway host: " << gateway["Host"].as<std::string>()
              << std::endl;
    std::cout << "Gateway Port: " << gateway["Port"].as<int>() << std::endl;

  } catch (const YAML::Exception &e) {
    std::cerr << "YAML exception: " << e.what() << std::endl;
    return 1;
  } catch (const std::runtime_error &e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
void foo2() {
  YAML::Node conf = YAML::LoadFile("config.yaml")["server"];

  auto node = conf["peerServer"].as<std::vector<YAML::Node>>();
  for (auto it : node) {
    std::cout << it["host"].as<std::string>() << ":"
              << it["port"].as<std::string>() << std::endl;
  }
}
int main() {
  // foo2();

  int q = 0, b = 20;

  std::cout << 2 % b;
  return 0;
}
