#pragma once

#include "spdlog/common.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace wim {

// 日志级别
#define LOG_DEBUG(logger, ...)                                                 \
  logger->debug("[{}:{}({})] \n{}", __FILE__, __LINE__, __FUNCTION__,          \
                fmt::format(__VA_ARGS__))

#define LOG_TRACE(logger, ...)                                                 \
  logger->trace("[{}:{}({})] \n{}", __FILE__, __LINE__, __FUNCTION__,          \
                fmt::format(__VA_ARGS__))

#define LOG_INFO(logger, ...)                                                  \
  logger->info("[{}:{}({})] \n{}", __FILE__, __LINE__, __FUNCTION__,           \
               fmt::format(__VA_ARGS__))

#define LOG_WARN(logger, ...)                                                  \
  logger->warn("[{}:{}({})] \n{}", __FILE__, __LINE__, __FUNCTION__,           \
               fmt::format(__VA_ARGS__))

#define LOG_ERROR(logger, ...)                                                 \
  logger->error("[{}:{}({})] \n{}", __FILE__, __LINE__, __FUNCTION__,          \
                fmt::format(__VA_ARGS__))

inline std::shared_ptr<spdlog::logger>
createDebugLogger(std::string name, std::string path, int files = 3,
                  spdlog::level::level_enum level = spdlog::level::debug) {
  // 控制台输出（带颜色）
  auto outSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  outSink->set_level(level);
  // outSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
  outSink->set_pattern("[%^%l%$] [thread %t] %v");

  // 文件输出（自动轮转）
  auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      path + "/" + name + ".log", 1024 * 1024 * 5, files); // 5MB x 3 files
  fileSink->set_level(level);
  fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

  auto logger = std::make_shared<spdlog::logger>(
      name, spdlog::sinks_init_list{outSink, fileSink});
  logger->set_level(level);
  return logger;
}

inline std::shared_ptr<spdlog::logger> dbLogger =
    createDebugLogger("db", "logs/db", spdlog::level::debug);

inline std::shared_ptr<spdlog::logger> netLogger =
    createDebugLogger("net", "logs/net", spdlog::level::debug);

inline std::shared_ptr<spdlog::logger> businessLogger =
    createDebugLogger("bussiness", "logs/bussiness", spdlog::level::debug);

inline void setLoggerLevel(spdlog::level::level_enum level) {
  dbLogger->set_level(level);
  netLogger->set_level(level);
  businessLogger->set_level(level);
}

inline std::string getLogLevelStr() {
  std::string ret{};
  ret += "\n存储日志级别：";
  ret += spdlog::level::to_string_view(dbLogger->level()).data();
  ret += "\n网络日志级别：";
  ret += spdlog::level::to_string_view(netLogger->level()).data();
  ret += "\n业务日志级别：";
  ret += spdlog::level::to_string_view(businessLogger->level()).data();
  ret += "\n";
  return ret;
}

}; // namespace wim