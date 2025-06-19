#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {

  std::cout << "argc: " << argc << "\n";
  for (int i = 0; i < argc; i++) {
    std::cout << argv[i] << " ";
  }
  std::cout << "\n";

  auto logger =
      spdlog::create<spdlog::sinks::stdout_color_sink_mt>("my_logger");
  logger->set_level(spdlog::level::info);
  logger->info("Hello, world!");
  logger->debug("This is a debug message");
}