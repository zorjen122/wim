#include "base.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <fcntl.h>
#include <spdlog/spdlog.h>

int main() {

  net::io_context ioc;
  auto sock = base::startChatClient(ioc, "127.0.0.1", "8090");

  int fd = open("../CMakeLists.txt", O_RDONLY);
  if (fd < 0) {
    spdlog::error("open file failed");
    return -1;
  }

  while (true) {
    char buf[1025]{};
    int n = read(fd, buf, 30);
    buf[n] = '\0';
    if (n == 0) {
      spdlog::info("read file end");
      break;
    }
    spdlog::info("read file: {}", buf);
    sock->send(net::buffer(buf, n));
  }
  ioc.run();
  close(fd);

  return 0;
}