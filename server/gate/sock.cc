#include "include/Session.h"
#include <bits/stdc++.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstring>

namespace wim {

struct TcpServer;

struct TcpServerSession : public Session {
  TcpServerSession(io_context &ioc, int sid, TcpServer *server)
      : Session(ioc), server(server) {
    SetSessionID(sid);
  }
  void ClearSession() override;
  void HandlePacket(std::shared_ptr<context> channel) override;
  TcpServer *server;
};

struct TcpServer {

  TcpServer(io_context &io_context, std::string port)
      : acceptor(io_context,
                 ip::tcp::endpoint(ip::tcp::v4(), std::stoi(port))) {
    static int sid = 1;
    spawn(acceptor.get_executor(), [this, &io_context](yield_context yield) {
      boost::system::error_code ec;
      while (io_context.stopped() == false) {
        auto session =
            std::make_shared<TcpServerSession>(io_context, sid++, this);
        acceptor.async_accept(session->GetSocket(), yield[ec]);
        if (ec) {
          std::cout << "accept error: " << ec.message() << std::endl;
          continue;
        }
        sessions[session->GetSessionID()] = session;
        session->Start();
      }
    });
  }

  ip::tcp::acceptor acceptor;
  void ClearSession(long sid) { sessions.erase(sid); }
  std::map<long, std::shared_ptr<TcpServerSession>> sessions;
}; // namespace wim

void TcpServerSession::ClearSession() { server->ClearSession(GetSessionID()); }
void TcpServerSession::HandlePacket(std::shared_ptr<context> context) {
  std::cout << "endpoint[ " << context->session->GetEndpointToString() << "]:\t"
            << context->packet->to_string() << "\n";
  return;
}
struct TcpClient : public Session {
  TcpClient(io_context &ioc, std::string port) : Session(ioc) {
    spawn(ioc, [this, port, &ioc](yield_context yield) {
      boost::system::error_code ec;
      ip::tcp::resolver resolver(ioc);
      auto results = resolver.resolve(port, ec);
      if (ec) {
        HandleError(ec);
        return;
      }
      auto &socket = this->GetSocket();
      if (results.empty()) {
        std::cout << "resolve error: " << ec.message() << std::endl;
        return;
      }
      auto ep = results.begin()->endpoint();
      socket.async_connect(ep, yield[ec]);
      if (ec) {
        HandleError(ec);
        return;
      }

      Session::Start();
      Start();
    });
  };

  void Start() {
    auto packet = protocol::to_packet(1, 1, 1, "hello");
    Send(packet);
    std::cout << "send packet: " << packet->to_string() << std::endl;
  }

  void ClearSession() override {}
  void HandlePacket(std::shared_ptr<context> context) override {
    std::cout << "endpoint[ " << context->session->GetEndpointToString();
  }

}; // namespace wim

io_context ioc;

int server(std::string port) {
  // 写一个最小代码简单的回声服务器

  // ip::tcp::acceptor acceptor(ioc, ip::tcp::endpoint(ip::tcp::v4(), 3030));
  // ip::tcp::socket client(ioc);
  // acceptor.accept(client);
  // std::string data;
  // char buf[1024];
  // auto len = client.read_some(buffer(buf, 1024));
  // if (len == 0) {
  //   return -1;
  // }
  // data.append(buf, len);
  // std::cout << "收到数据：" << data << std::endl;

  std::unique_ptr<io_context::work> worker(new io_context::work(ioc));
  std::shared_ptr<TcpServer> server(new TcpServer(ioc, port));

  std::atomic<bool> sigint(false);
  std::thread service_control([&worker, &sigint]() {
    auto tmp = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(tmp);
    auto expire_time = std::mktime(std::localtime(&now_time)) + 60;
    std::cout << "服务开放时间：" << std::ctime(&now_time) << " 至 "
              << std::ctime(&expire_time) << " 止" << std::endl;
    int count = 0;
    while (count < 60 && !sigint) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      ++count;
    }
    worker.reset();
    ioc.stop();
  });

  boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait(
      [&worker, &sigint](const boost::system::error_code &ec, int signo) {
        if (!ec) {
          std::cout << "收到信号：" << signo << std::endl;
          sigint.store(true);
          worker.reset();
          ioc.stop();
        }
      });

  ioc.run();
  service_control.join();
  return 0;
}

int client(std::string port) {
  std::unique_ptr<io_context::work> worker(new io_context::work(ioc));
  std::shared_ptr<TcpClient> client(new TcpClient(ioc, port));

  std::atomic<bool> sigint(false);
  std::thread service_control([&worker, &sigint]() {
    auto tmp = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(tmp);
    auto expire_time = std::mktime(std::localtime(&now_time)) + 20;
    std::cout << "服务开放时间：" << std::ctime(&now_time) << " 至 "
              << std::ctime(&expire_time) << " 止" << std::endl;
    int count = 0;
    while (count < 20 && !sigint) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      ++count;
    }
    worker.reset();
    ioc.stop();
  });

  boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait(
      [&worker, &sigint](const boost::system::error_code &ec, int signo) {
        if (!ec) {
          std::cout << "收到信号：" << signo << std::endl;
          sigint.store(true);
          worker.reset();
          ioc.stop();
        }
      });

  ioc.run();
  service_control.join();

  return 0;
}
}; // namespace wim

int main(int argc, char *argv[]) {
  if (argc > 2 && std::string(argv[1]) == "server") {
    return wim::server(argv[2]);
  } else if (argc > 2 && std::string(argv[1]) == "client") {
    return wim::client(argv[2]);
  }
}