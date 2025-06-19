#include "ImSession.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace wim {

namespace net {
using namespace boost::asio;
using namespace ip;
}; // namespace net
using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

class ImGateway : public std::enable_shared_from_this<ImGateway> {
public:
  typedef websocketpp::server<websocketpp::config::asio> WsServer;
  typedef std::map<connection_hdl, uint64_t, std::owner_less<connection_hdl>>
      ConnectionMap;
  using Ptr = std::shared_ptr<ImGateway>;
  ImGateway(uint16_t ws_port);

  void start();

  // 发送WebSocket响应
  void send_ws_response(uint64_t conn_id, const std::string &payload);

private:
  // WebSocket服务器初始化
  void init_websocket_server();

  // WebSocket连接建立
  void on_ws_open(connection_hdl hdl);

  // WebSocket连接关闭
  void on_ws_close(connection_hdl hdl);

  // WebSocket消息处理
  void on_ws_message(connection_hdl hdl, WsServer::message_ptr msg);

  // 运行IO上下文
  void run_io_context();

  // 成员变量
  uint16_t ws_port_;

  net::io_context io_context_;
  WsServer ws_server_;

  std::atomic<uint64_t> next_conn_id_{1};

  ConnectionMap connections_;
  std::map<uint64_t, connection_hdl> conn_id_to_hdl_;

  std::mutex connections_mutex_;
  std::mutex pending_requests_mutex_;

  ImSessionManager::Ptr im_sessions;
};

}; // namespace wim
