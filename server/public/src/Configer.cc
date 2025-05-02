#include "Configer.h"
#include "Logger.h"

std::unordered_map<std::string, YAML::Node> Configer::configMap = {};

bool Configer::loadConfig(const std::string &configFile) {
  try {
    YAML::Node config = YAML::LoadFile(configFile);
    if (config.IsNull()) {
      LOG_WARN(wim::businessLogger, "Config file is empty, configFile: {}",
               configFile);
      return false;
    }

    for (auto it = config.begin(); it != config.end(); ++it) {
      configMap[it->first.as<std::string>()] = it->second;
    }

    LOG_INFO(wim::businessLogger, "Config file loaded, configFile: {}",
             configFile);
    return true;
  } catch (const YAML::ParserException &e) {
    spdlog::error("Error load config file: {}", e.what());
    return false;
  }
}

YAML::Node Configer::getNode(const std::string &filed) {
  auto it = configMap.find(filed);
  if (it == configMap.end()) {
    LOG_DEBUG(wim::businessLogger, "Config not found, filed: {}", filed);
    return YAML::Node();
  }

  return it->second;
}

std::string Configer::getSaveFilePath() { return "./"; }
