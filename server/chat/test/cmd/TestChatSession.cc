
#include "ChatServer.h"
#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>
#include <stdexcept>

namespace wim {
void test() {
  net::io_context ioc;

  tcp::socket socket(ioc);

  boost::system::error_code ec;
  socket.connect(
      tcp::endpoint(net::ip::address::from_string("127.0.0.1"), 8080), ec);
  if (ec.failed()) {
    throw std::runtime_error("Failed to connect to server: " + ec.message());
  }

  socket.send(net::buffer("Hello, world!"), 0, ec);
  if (ec.failed()) {
    throw std::runtime_error("Failed to send message: " + ec.message());
  }

  char data[1024];
  size_t len = socket.receive(net::buffer(data), 0, ec);
  if (ec.failed()) {
    throw std::runtime_error("Failed to receive message: " + ec.message());
  }

  std::string message(data, len);
}
} // namespace wim

int main() { return 0; }