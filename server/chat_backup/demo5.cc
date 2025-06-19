#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>

namespace net = boost::asio;
std::shared_ptr<net::steady_timer> messageReadTimer{};
net::io_context ioc;
void func() {
  bool isFirst = messageReadTimer == nullptr;
  if (isFirst) {
    messageReadTimer =
        std::make_shared<net::steady_timer>(ioc, std::chrono::seconds(3));
  }

  // 更准确地检查定时器是否在运行
  bool isTimerActive =
      (messageReadTimer->expires_from_now() > std::chrono::seconds(0));

  if (isFirst || !isTimerActive) {
    messageReadTimer->expires_after(std::chrono::seconds(3));
    messageReadTimer->async_wait([](const boost::system::error_code &ec) {
      if (ec == boost::asio::error::operation_aborted) {
        std::cout << "timer cancelled\n";
      } else {
        std::cout << "timer expired\n";
      }
    });
  } else {
    auto expiry_time = messageReadTimer->expiry();
    auto now = net::steady_timer::clock_type::now();
    std::cout << "Timer is active, will expire in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     expiry_time - now)
                     .count()
              << " ms\n";
  }
}

int main() {
  std::thread t([]() {
    int cnt = 0;
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (cnt == 10)
        break;
      func();

      std::cout << "tickle: " << cnt++ << "\n";
    }
  });
  if (t.joinable())
    t.join();
  ioc.run();
  return 0;
}