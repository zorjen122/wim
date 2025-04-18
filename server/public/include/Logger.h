#pragma once

#include "spdlog/common.h"
#include "spdlog/spdlog.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace wim {

// 日志级别
#define LOG_DEBUG(logger, ...)                                                 \
  logger->debug("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))

#define LOG_TRACE(logger, ...)                                                 \
  logger->trace("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))

#define LOG_INFO(logger, ...)                                                  \
  logger->info("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))

#define LOG_WARN(logger, ...)                                                  \
  logger->warn("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))

#define LOG_ERROR(logger, ...)                                                 \
  logger->error("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))

static std::shared_ptr<spdlog::logger>
createDebugLogger(std::string name, std::string path,
                  spdlog::level::level_enum level = spdlog::level::debug) {
  // 控制台输出（带颜色）
  auto outSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  outSink->set_level(level);
  outSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

  // 文件输出（自动轮转）
  auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      path + "/" + name + ".log", 1024 * 1024 * 5, 3); // 5MB x 3 files
  fileSink->set_level(level);
  fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

  auto logger = std::make_shared<spdlog::logger>(
      name, spdlog::sinks_init_list{outSink, fileSink});
  logger->set_level(level);
  return logger;
}

static std::shared_ptr<spdlog::logger> dbLogger =
    createDebugLogger("db", "logs/db", spdlog::level::info);

static std::shared_ptr<spdlog::logger> netLogger =
    createDebugLogger("net", "logs/net", spdlog::level::info);

static std::shared_ptr<spdlog::logger> businessLogger =
    createDebugLogger("bussiness", "logs/bussiness", spdlog::level::info);

}; // namespace wim