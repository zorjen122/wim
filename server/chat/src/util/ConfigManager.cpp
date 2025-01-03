#include "ConfigManager.h"

namespace ConfigManager{

  std::unordered_map<std::string, YAML::Node> configMap{};

  bool loadConfig(const std::string &configFile) {
    try {
      YAML::Node config = YAML::LoadFile(configFile);
      if (config.IsNull()) {
        return false;
      }
      for (auto it = config.begin(); it != config.end(); ++it) {
        configMap[it->first.as<std::string>()] = it->second;
      }

      return true;
    } catch (const YAML::ParserException &e) {
      spdlog::error("Error load config file: {}", e.what());
      return false;
    }
  }

  YAML::Node getConfig(const std::string &configName)
  {
    auto it = configMap.find(configName);
    if (it == configMap.end()) {
      return YAML::Node();
    }
    return it->second;
  }
  
  bool hasConfig(const std::string &configName)
  {
    auto it = configMap.find(configName);
    return it == configMap.end();
  }
};