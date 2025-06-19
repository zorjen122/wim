#include "ImSession.h"
#include "ImGateway.h"
#include "IocPool.h"
#include "Logger.h"
#include "Session.h"
#include "StateRpc.h"
#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <exception>
#include <jsoncpp/json/json.h>
#include <string>

namespace wim {
ImSession::ImSession(net::io_context &io_context,
                     const rpc::EndPoint &remote_endpoint,
                     std::shared_ptr<ImGateway> im_gateway)
    : Session(io_context), im_gateway(im_gateway) {

  boost::system::error_code ec;

  std::string name = remote_endpoint.name(), ip = remote_endpoint.ip();
  uint16_t port = remote_endpoint.port();

  net::ip::tcp::endpoint ep(net::ip::address::from_string(ip), port);

  auto &socket = this->GetSocket();
  ec = socket.connect(ep, ec);
  if (ec.failed())
    throw std::runtime_error("通讯服务器连接失败: " + ec.message());

  LOG_INFO(wim::netLogger,
           "建立通讯服务器网络链路，地址: {}, 端口号: {}，服务器标识：{}", ip,
           port, name);
}

void ImSession::HandlePacket(std::shared_ptr<Session::context> channel) {
  auto caller = im_gateway.lock();
  if (!caller) {
    auto packet = channel->packet;
    auto p = protocol::to_packet(packet->from, packet->device, packet->id,
                                 packet->data);
    std::string response(p->data, p->total);
    caller->send_ws_response(packet->from, response);
  }
}

void ImSession::ClearSession() {}

ImSessionManager::ImSessionManager(std::shared_ptr<ImGateway> im_gateway) {

  endpoints = rpc::StateRpc::GetInstance()->PullImNodeList();
  if (endpoints.endpoints_size() == 0) {
    LOG_ERROR(netLogger, "没有可用的通讯服务器节点");
    return;
  }
  LOG_INFO(netLogger, "共加载 {} 个通讯服务器节点", endpoints.endpoints_size());
  size_t size = endpoints.endpoints_size(), count = 0;
  for (auto &endpoint : endpoints.endpoints()) {

    try {
      auto &io_context = IocPool::GetInstance()->GetContext();

      ImSession::Ptr session(new ImSession(io_context, endpoint, im_gateway));
      sessions[endpoint.name()] = session;
      sessionIds.push_back(endpoint.name());
      ++count;
      LOG_INFO(netLogger, "节点({})：{}:{} 加载成功", endpoint.name(),
               endpoint.ip(), endpoint.port());
    } catch (std::exception &e) {
      LOG_WARN(netLogger, "节点({})：{}:{} 加载失败，异常：{}", endpoint.name(),
               endpoint.ip(), endpoint.port(), e.what());
      continue;
    }
  }
  LOG_INFO(netLogger, "初始化通讯服务，共加载 {} 个节点，成功加载 {} 个节点",
           size, count);
}
std::shared_ptr<ImSession>
ImSessionManager::getSession(const std::string &sessionId) {
  return sessions[sessionId];
}
void ImSessionManager::eraseSession(const std::string &sessionId) {
  if (sessions.count(sessionId)) {
    sessions.erase(sessionId);
    sessionIds.erase(
        std::remove(sessionIds.begin(), sessionIds.end(), sessionId));
  } else {
    LOG_WARN(netLogger, "没有这样的通讯服务器会话：{}", sessionId);
  }
}
std::shared_ptr<ImSession> ImSessionManager::getSession() {
  static short count = 0;
  if (count >= sessionIds.size())
    count = 0;
  return sessions[sessionIds[count++]];
}

void ImSessionManager::addSession(const std::string &sessionId,
                                  std::shared_ptr<ImSession> session) {
  if (sessions.count(sessionId)) {
    LOG_WARN(netLogger, "已经存在这样的通讯服务器会话：{}", sessionId);
    return;
  }
  sessions[sessionId] = session;
  sessionIds.push_back(sessionId);
}
ImSessionManager::~ImSessionManager() {
  for (auto &session : sessions) {
    session.second->ClearSession();
  }
}

} // namespace wim