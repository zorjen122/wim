#include "GateServer.h"

#include "HttpSession.h"
#include "IocPool.h"
#include <spdlog/spdlog.h>

GateServer::GateServer(net::io_context &ioc, unsigned short &port)
    : gateContext(ioc), acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {}

void GateServer::Start() {
  auto self = shared_from_this();
  auto &iocontext = IocPool::GetInstance()->GetContext();
  auto connection = std::make_shared<HttpSession>(iocontext);
  acceptor.async_accept(connection->GetSocket(), [self, connection](
                                                     beast::error_code ec) {
    try {
      if (ec) {
        self->Start();
        spdlog::info("accept-restart: connect is wrong, ec as {}", ec.what());
        return;
      }

      connection->Start();
      self->Start();
    } catch (std::exception &err) {
      spdlog::warn("exception is {}", err.what());

      self->Start();
    }
  });
}
