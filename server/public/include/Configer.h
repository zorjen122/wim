#pragma once
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace Configer {

extern std::unordered_map<std::string, YAML::Node> configMap;
extern bool loadConfig(const std::string &configFile);
extern YAML::Node getConfig(const std::string &configName);
extern bool hasConfig(const std::string &configName);

}; // namespace Configer