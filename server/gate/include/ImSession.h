#pragma once
#include "Const.h"
#include "Session.h"
#include "StateRpc.h"
#include <memory>

namespace wim {

class ImGateway;

class ImSession final : public Session {
public:
  using Ptr = std::shared_ptr<ImSession>;
  ImSession(io_context &io_context, const rpc::EndPoint &remote_endpoint,
            std::shared_ptr<ImGateway> im_gateway);
  void HandlePacket(std::shared_ptr<Session::context> channel) override;
  void ClearSession() override;

private:
  std::weak_ptr<ImGateway> im_gateway;
};

class ImSessionManager {
public:
  using Ptr = std::unique_ptr<ImSessionManager>;
  ImSessionManager(std::shared_ptr<ImGateway> im_gateway);
  std::shared_ptr<ImSession> getSession(const std::string &sessionId);
  std::shared_ptr<ImSession> getSession();

  void eraseSession(const std::string &sessionId);
  void addSession(const std::string &sessionId,
                  std::shared_ptr<ImSession> session);
  ~ImSessionManager();

private:
  std::unordered_map<std::string, ImSession::Ptr> sessions;
  std::vector<std::string> sessionIds;
  rpc::EndPointList endpoints;
};
}; // namespace wim