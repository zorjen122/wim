#include "SessionRegistry.h"

#include "GatewaySession.h"
#include "Logger.h"
#include "TcpMessageCodec.h"

#include <utility>

namespace wim::connection {

SessionRegistry::SessionRegistry(std::string gatewayId, std::string instanceId,
                                 long leaseTtlSeconds)
    : gatewayId(std::move(gatewayId)),
      instanceId(std::move(instanceId)),
      leaseTtlSeconds(leaseTtlSeconds) {}

db::SessionLease SessionRegistry::Bind(
    int64_t uid, const std::shared_ptr<GatewaySession> &session) {
  db::SessionLease lease;
  lease.gatewayId = gatewayId;
  lease.instanceId = instanceId;
  lease.connectionId = session->ConnectionId();
  lease.generation = db::RedisDao::GetInstance()->bindSessionLease(
      uid, gatewayId, instanceId, lease.connectionId, leaseTtlSeconds);
  if (lease.generation <= 0)
    return {};

  // rebind
  std::shared_ptr<GatewaySession> oldSession;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto found = sessions.find(uid);
    if (found != sessions.end())
      oldSession = found->second.session.lock();
    sessions[uid] = LocalSession{session, lease};
  }
  if (oldSession && oldSession != session)
    oldSession->Close();
  return lease;
}

bool SessionRegistry::Refresh(int64_t uid, const db::SessionLease &lease) {
  return db::RedisDao::GetInstance()->refreshSessionLease(uid, lease,
                                                          leaseTtlSeconds);
}

void SessionRegistry::Remove(int64_t uid,
                             const std::shared_ptr<GatewaySession> &session,
                             const db::SessionLease &lease) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto found = sessions.find(uid);
    if (found == sessions.end())
      return;
    auto current = found->second.session.lock();
    if (current && current != session)
      return;
    sessions.erase(found);
  }
  db::RedisDao::GetInstance()->clearSessionLease(uid, lease);
}

gateway::DeliveryStatus SessionRegistry::Deliver(
    const gateway::DeliveryEnvelope &delivery) {
  std::shared_ptr<GatewaySession> session;
  db::SessionLease lease;
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto found = sessions.find(delivery.recipient_uid());
    if (found == sessions.end())
      return gateway::DELIVERY_STATUS_OFFLINE;
    session = found->second.session.lock();
    lease = found->second.lease;
  }
  if (!session)
    return gateway::DELIVERY_STATUS_OFFLINE;
  if (lease.connectionId != delivery.expected_connection_id() ||
      lease.generation != delivery.expected_connection_generation())
    return gateway::DELIVERY_STATUS_STALE_ROUTE;
  TcpPacket packet;
  const bool hasTransportSequence = ParseTcpPacket(delivery.packet(), packet) &&
                                    packet.has_seq() && packet.seq() > 0;
  const bool queued =
      hasTransportSequence
          ? session->SendReliable(delivery.packet(), delivery.protocol_id(),
                                  packet.seq())
          : session->SendRaw(delivery.packet(), delivery.protocol_id());
  if (!queued)
    return gateway::DELIVERY_STATUS_BACKPRESSURED;
  return gateway::DELIVERY_STATUS_QUEUED;
}

const std::string &SessionRegistry::GatewayId() const {
  return gatewayId;
}

const std::string &SessionRegistry::InstanceId() const {
  return instanceId;
}

}  // namespace wim::connection
