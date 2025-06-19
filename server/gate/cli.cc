#include <netinet/in.h>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <iostream>
#include <string>

typedef websocketpp::client<websocketpp::config::asio_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::thread> thread_ptr;

// 客户端类
class WebSocketClient {
public:
  WebSocketClient() {
    // 初始化客户端
    m_client.init_asio();

    // 设置日志级别（可选）
    m_client.set_access_channels(websocketpp::log::alevel::none);
    m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // 绑定事件处理器
    m_client.set_open_handler(
        [this](websocketpp::connection_hdl hdl) { on_open(hdl); });
    m_client.set_message_handler(
        [this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
          on_message(hdl, msg);
        });
    m_client.set_close_handler(
        [this](websocketpp::connection_hdl hdl) { on_close(hdl); });
    m_client.set_fail_handler(
        [this](websocketpp::connection_hdl hdl) { on_fail(hdl); });
  }

  // 连接到服务器
  void connect(const std::string &uri) {
    websocketpp::lib::error_code ec;
    client::connection_ptr conn = m_client.get_connection(uri, ec);
    if (ec) {
      std::cerr << "连接错误: " << ec.message() << std::endl;
      return;
    }

    m_hdl = conn->get_handle();
    m_client.connect(conn);

    // 启动异步IO线程
    m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(
        &client::run, &m_client);
  }

  // 发送消息
  void send(const std::string &message) {
    websocketpp::lib::error_code ec;
    m_client.send(m_hdl, message, websocketpp::frame::opcode::text, ec);
    if (ec) {
      std::cerr << " 数据发送错误: " << ec.message() << std::endl;
    }
  }
  void send_binary(const void *data, size_t length) {
    websocketpp::lib::error_code ec;
    m_client.send(m_hdl, data, length, websocketpp::frame::opcode::binary, ec);
    if (ec) {
      std::cerr << "发送错误: " << ec.message() << std::endl;
    }
  }

  // 关闭连接
  void close() {
    m_client.close(m_hdl, websocketpp::close::status::normal, "客户端关闭");
    if (m_thread) {
      m_thread->join();
    }
  }

private:
  client m_client;
  websocketpp::connection_hdl m_hdl;
  thread_ptr m_thread;

  // 连接成功回调
  void on_open(websocketpp::connection_hdl hdl) {
    std::cout << "连接已建立" << std::endl;
  }

  // 接收消息回调
  void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    std::cout << "收到消息: " << msg->get_payload() << std::endl;
  }

  // 连接关闭回调
  void on_close(websocketpp::connection_hdl hdl) {
    std::cout << "连接已关闭" << std::endl;
  }

  // 连接失败回调
  void on_fail(websocketpp::connection_hdl hdl) {
    std::cerr << "连接失败" << std::endl;
  }
};

#include <jsoncpp/json/json.h>

int main() {
  WebSocketClient ws_client;
  ws_client.connect("ws://localhost:2020");
  std::this_thread::sleep_for(std::chrono::seconds(1)); // 等待连接

  // 构造二进制消息
  uint8_t buf[1024]{};
  uint32_t id = htonl(1013);

  Json::Value data;
  data["uid"] = Json::Value::Int64(303817497906253825);
  std::string json_str = data.toStyledString();
  uint32_t len = htonl(json_str.size());

  // 填充缓冲区（二进制安全）
  size_t offset = 0;
  memcpy(buf + offset, &id, sizeof(id));
  offset += sizeof(id);
  memcpy(buf + offset, &len, sizeof(len));
  offset += sizeof(len);
  memcpy(buf + offset, json_str.data(), json_str.size());
  offset += json_str.size();

  // 以二进制帧发送
  ws_client.send_binary(buf, offset);

  std::this_thread::sleep_for(std::chrono::seconds(10));
  ws_client.close();
  return 0;
}