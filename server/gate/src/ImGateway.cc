#include "ImGateway.h"
#include "Const.h"
#include "ImSession.h"
#include <boost/asio.hpp>
#include <cstdint>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <map>
#include <mutex>
#include <thread>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
namespace wim {

using namespace boost::asio;
using namespace ip;
using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

typedef websocketpp::server<websocketpp::config::asio> WsServer;
typedef std::map<connection_hdl, uint64_t, std::owner_less<connection_hdl>>
    ConnectionMap;

ImGateway::ImGateway(uint16_t ws_port)
    : ws_port_(ws_port), io_context_(), ws_server_() {}
void ImGateway::start() {
  im_sessions.reset(new ImSessionManager(shared_from_this()));
  init_websocket_server();
  run_io_context();
}
// WebSocket服务器初始化
void ImGateway::init_websocket_server() {
  ws_server_.init_asio(&io_context_);
  ws_server_.set_reuse_addr(true);

  // 设置WebSocket事件处理
  ws_server_.set_open_handler(bind(&ImGateway::on_ws_open, this, _1));
  ws_server_.set_close_handler(bind(&ImGateway::on_ws_close, this, _1));
  ws_server_.set_message_handler(bind(&ImGateway::on_ws_message, this, _1, _2));

  ws_server_.listen(ws_port_);
  ws_server_.start_accept();
}

// WebSocket连接建立
void ImGateway::on_ws_open(connection_hdl hdl) {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  uint64_t conn_id = next_conn_id_++;
  connections_[hdl] = conn_id;
  conn_id_to_hdl_[conn_id] = hdl;

  std::cout << "WebSocket connection opened, ID: " << conn_id << std::endl;
}

// WebSocket连接关闭
void ImGateway::on_ws_close(connection_hdl hdl) {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  auto it = connections_.find(hdl);
  if (it != connections_.end()) {
    uint64_t conn_id = it->second;
    connections_.erase(it);
    conn_id_to_hdl_.erase(conn_id);
    std::cout << "WebSocket connection closed, ID: " << conn_id << std::endl;
  }
}

// WebSocket消息处理
void ImGateway::on_ws_message(connection_hdl hdl, WsServer::message_ptr msg) {
  if (msg->get_opcode() != websocketpp::frame::opcode::binary) {
    ws_server_.close(hdl, websocketpp::close::status::invalid_payload,
                     "Binary messages only");
    return;
  }

  const std::string &payload = msg->get_payload();
  // 提取服务ID (前4字节)
  uint32_t service_id =
      htonl(*reinterpret_cast<const uint32_t *>(payload.data()));

  // pull handle

  im_sessions->getSession()->Send(payload.data() + PROTOCOL_HEADER_TOTAL,
                                  service_id);
}

// 发送WebSocket响应
void ImGateway::send_ws_response(uint64_t conn_id, const std::string &payload) {
  connection_hdl hdl;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = conn_id_to_hdl_.find(conn_id);
    if (it == conn_id_to_hdl_.end()) {
      std::cerr << "Connection ID " << conn_id << " not found" << std::endl;
      return;
    }
    hdl = it->second;
  }

  try {
    ws_server_.send(hdl, payload.data(), payload.size(),
                    websocketpp::frame::opcode::binary);
  } catch (const std::exception &e) {
    std::cerr << "WebSocket send error: " << e.what() << std::endl;
  }
}

// 运行IO上下文
void ImGateway::run_io_context() {
  auto worker = [this] {
    try {
      net::io_context::work work(io_context_);
      io_context_.run();
    } catch (const std::exception &e) {
      std::cerr << "IO context error: " << e.what() << std::endl;
    }
  };

  // 启动IO线程池
  worker();
  // size_t thread_count = std::thread::hardware_concurrency();
  // std::vector<std::thread> threads;
  // for (size_t i = 0; i < thread_count; ++i) {
  //   threads.emplace_back(worker);
  // }

  // for (auto &t : threads) {
  //   t.join();
  // }
}

}; // namespace wim
