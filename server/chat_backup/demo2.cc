#include <bits/stdc++.h>
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>

namespace net = boost::asio;
std::shared_ptr<net::steady_timer> timer{};

void ping(net::io_context &ioc, int count = 0) {
  // 使用智能指针管理定时器
  timer = std::make_shared<net::steady_timer>(ioc, std::chrono::seconds(2));

  timer->async_wait([&ioc, count](const boost::system::error_code &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      std::cout << "Timer cancelled" << std::endl;
    } else {
      std::cout << "Timer expired" << std::endl;
      static const int retry_max = 3;
      if (count + 1 >= retry_max) {
        std::cout << "Retry limit reached" << std::endl;
      } else {
        std::cout << "Retrying..." << std::endl;
        ping(ioc, count + 1);
      }
    }
  });
}

int main() {
  net::io_context ioc;
  ping(ioc);

  std::thread t([]() {
    int count = 0;
    while (true) {
      if (count == 5) {
        timer.reset();
        break;
      }
      sleep(1);
      std::cout << "tickle\n";
      ++count;
    }
  });

  ioc.run();
  t.join();
}
