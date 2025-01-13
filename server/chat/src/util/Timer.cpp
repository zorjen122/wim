#include "Timer.h"
#include <boost/asio/detail/chrono.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>

Timer::Timer(boost::asio::io_context &ioContext, size_t second)
    : ioc(ioContext), timer(ioContext, boost::asio::chrono::seconds(second)) {
  startTimer();
}

Timer::~Timer() {}
void Timer::startTimer() {
  timer.async_wait([this](const boost::system::error_code &ec) {
    if (!ec) {
      spdlog::info("[Timer clicked]");

      if (resetHandle()) {
        spdlog::info("[Resetting timer]");
        resetTimer();
      }
    } else if (ec == boost::asio::error::timed_out) {
      tickleHandle();
    }
  });
}
void Timer::resetTimer() {
  timer.cancel();
  // 重新设置定时器时间
  timer = steady_timer(ioc, boost::asio::chrono::seconds(3)); // 例如，重置为3秒
  startTimer();
}

bool Timer::resetHandle() {
  spdlog::info("Timer::shouldResetTimer");
  return true;
}
void Timer::tickleHandle() { spdlog::info("Timer::timeoutHandle"); }