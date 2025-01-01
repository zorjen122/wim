#include "GateServer.h"

#include "HttpConnection.h"
#include "IOServicePool.h"
#include "spdlog/spdlog.h"

GateServer::GateServer(boost::asio::io_context &ioc, unsigned short &port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {}

void GateServer::Start() {
  auto self = shared_from_this();
  auto &io_context = AsioIOServicePool::GetInstance()->GetIOService();
  auto new_connection = std::make_shared<HttpConnection>(io_context);
  _acceptor.async_accept(new_connection->GetSocket(),
                         [self, new_connection](beast::error_code ec) {
                           try {
                             if (ec) {
                               self->Start();
                               spdlog::info(
                                   "accept-restart: connect is wrong, ec as {}",
                                   ec.message());
                               return;
                             }

                             new_connection->Start();
                             self->Start();
                           } catch (std::exception &err) {
                             spdlog::error("exception is {}", err.what());

                             self->Start();
                           }
                         });
}
