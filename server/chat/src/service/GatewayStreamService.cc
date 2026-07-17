#include "GatewayStreamService.h"

#include "Const.h"
#include "Logger.h"
#include "Redis.h"
#include "RequestContext.h"
#include "Service.h"
#include "TcpMessageCodec.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <utility>

namespace wim::rpc {
namespace {

constexpr std::size_t kMaxStreamQueue = 4096;

int64_t NowUnixMilliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

class GatewayStreamReactor final
    : public grpc::ServerBidiReactor<gateway::GatewayToMessageFrame,
                                     gateway::MessageToGatewayFrame> {
 public:
  GatewayStreamReactor(GatewayStreamService &service,
                       std::string authenticatedPeer)
      : service(service), authenticatedPeer(std::move(authenticatedPeer)) {
    StartRead(&readFrame);
  }

  bool Enqueue(gateway::MessageToGatewayFrame frame) {
    bool startWrite = false;
    {
      std::lock_guard<std::mutex> lock(writeMutex);
      if (finishing || writeQueue.size() >= kMaxStreamQueue)
        return false;
      writeQueue.push_back(std::move(frame));
      if (!writeInFlight) {
        writeInFlight = true;
        writeFrame = std::move(writeQueue.front());
        writeQueue.pop_front();
        startWrite = true;
      }
    }
    if (startWrite)
      StartWrite(&writeFrame);
    return true;
  }

  void OnReadDone(bool ok) override {
    if (!ok) {
      FinishOnce();
      return;
    }

    if (readFrame.has_register_gateway()) {
      HandleRegister(readFrame.register_gateway());
    } else if (readFrame.has_command()) {
      HandleCommand(readFrame.command());
    } else if (readFrame.has_heartbeat()) {
      gateway::MessageToGatewayFrame response;
      auto *heartbeat = response.mutable_heartbeat_ack();
      heartbeat->set_sent_at_unix_ms(readFrame.heartbeat().sent_at_unix_ms());
      heartbeat->set_sequence(readFrame.heartbeat().sequence());
      Enqueue(std::move(response));
    }

    readFrame.Clear();
    StartRead(&readFrame);
  }

  void OnWriteDone(bool ok) override {
    if (!ok) {
      FinishOnce();
      return;
    }
    bool startWrite = false;
    {
      std::lock_guard<std::mutex> lock(writeMutex);
      if (!writeQueue.empty()) {
        writeFrame = std::move(writeQueue.front());
        writeQueue.pop_front();
        startWrite = true;
      } else {
        writeInFlight = false;
      }
    }
    if (startWrite)
      StartWrite(&writeFrame);
  }

  void OnDone() override {
    service.Unregister(gatewayId, instanceId, this);
    delete this;
  }

 private:
  void HandleRegister(const gateway::RegisterGateway &request) {
    gatewayId = request.gateway_id();
    instanceId = request.instance_id();
    streamEpoch = request.stream_epoch();
    const bool valid =
        request.protocol_version() == 1 && !gatewayId.empty() &&
        !instanceId.empty() && !service.Draining() &&
        (!service.RequirePeerIdentity() || authenticatedPeer == gatewayId);
    if (valid) {
      service.Register(gatewayId, instanceId, this);
      registered = true;
    }

    gateway::MessageToGatewayFrame response;
    auto *result = response.mutable_register_result();
    result->set_accepted(valid);
    result->set_message_node_id(service.MessageNodeId());
    result->set_stream_epoch(streamEpoch);
    if (!valid)
      result->set_reason(
          "unsupported protocol or gateway identity does not match mTLS peer");
    Enqueue(std::move(response));
  }

  void HandleCommand(gateway::CommandEnvelope command) {
    const std::string requestId = command.request_id();
    const uint32_t serviceId = command.service_id();
    if (!registered || requestId.empty()) {
      gateway::MessageToGatewayFrame responseFrame;
      auto *response = responseFrame.mutable_command_result();
      response->set_request_id(requestId);
      response->set_response_service_id(
          __getServiceResponseId(ServiceID(serviceId)));
      response->set_error(ErrorCodes::AuthenticationRequired);
      response->set_retryable(false);
      response->set_packet(SerializeTcpPacket(MakeErrorPacket(
          ErrorCodes::AuthenticationRequired,
          "gateway stream must register before sending commands")));
      Enqueue(std::move(responseFrame));
      return;
    }
    if (command.deadline_unix_ms() <= NowUnixMilliseconds()) {
      gateway::MessageToGatewayFrame responseFrame;
      auto *response = responseFrame.mutable_command_result();
      response->set_request_id(requestId);
      response->set_response_service_id(
          __getServiceResponseId(ServiceID(serviceId)));
      response->set_error(ErrorCodes::DeadlineExceeded);
      response->set_retryable(true);
      response->set_packet(SerializeTcpPacket(MakeErrorPacket(
          ErrorCodes::DeadlineExceeded, "command deadline already elapsed")));
      Enqueue(std::move(responseFrame));
      return;
    }
    const std::string originGatewayId = gatewayId;
    const std::string originInstanceId = instanceId;
    auto *originReactor = this;
    auto *streamService = &service;
    auto accepted = Service::GetInstance()->PostBackgroundTask(
        [streamService, originGatewayId, originInstanceId, originReactor,
         command = std::move(command)]() mutable {
          const auto remaining = std::chrono::milliseconds(
              command.deadline_unix_ms() - NowUnixMilliseconds());
          RequestContext context = RequestContext::WithTimeout(
              command.request_id().empty() ? RequestContext::NextRequestId()
                                           : command.request_id(),
              getServiceIdString(command.service_id()), RequestSource::Rpc,
              command.actor_uid(), remaining);
          RequestContextScope contextScope(context);

          TcpPacket packet;
          gateway::MessageToGatewayFrame responseFrame;
          auto *response = responseFrame.mutable_command_result();
          response->set_request_id(command.request_id());
          response->set_response_service_id(
              __getServiceResponseId(ServiceID(command.service_id())));

          if (context.Expired()) {
            auto error = MakeErrorPacket(
                ErrorCodes::DeadlineExceeded,
                "command expired while waiting for a message worker");
            response->set_error(ErrorCodes::DeadlineExceeded);
            response->set_retryable(true);
            response->set_packet(SerializeTcpPacket(error));
            streamService->Reply(originGatewayId, originInstanceId,
                                 originReactor, std::move(responseFrame));
            return;
          }

          auto actorLease =
              db::RedisDao::GetInstance()->getSessionLease(command.actor_uid());
          if (actorLease.empty() || actorLease.gatewayId != originGatewayId ||
              actorLease.instanceId != originInstanceId ||
              actorLease.connectionId != command.connection_id() ||
              actorLease.generation != command.connection_generation()) {
            auto error =
                MakeErrorPacket(ErrorCodes::AuthenticationRequired,
                                "connection generation is no longer current");
            response->set_error(ErrorCodes::AuthenticationRequired);
            response->set_retryable(false);
            response->set_packet(SerializeTcpPacket(error));
            streamService->Reply(originGatewayId, originInstanceId,
                                 originReactor, std::move(responseFrame));
            return;
          }

          if (!ParseTcpPacket(command.packet(), packet)) {
            auto error = MakeErrorPacket(ErrorCodes::JsonParser);
            response->set_error(ErrorCodes::JsonParser);
            response->set_retryable(false);
            response->set_packet(SerializeTcpPacket(error));
            streamService->Reply(originGatewayId, originInstanceId,
                                 originReactor, std::move(responseFrame));
            return;
          }
          packet.set_uid(command.actor_uid());

          if (command.service_id() == ID_TEXT_SEND_REQ) {
            auto acceptedText = Service::GetInstance()->Messages().AcceptText(
                std::move(packet));
            response->set_error(TcpPacketError(acceptedText.response));
            response->set_retryable(acceptedText.response.retryable());
            response->set_packet(SerializeTcpPacket(acceptedText.response));
            streamService->Reply(originGatewayId, originInstanceId,
                                 originReactor, std::move(responseFrame));

            if (acceptedText.shouldDeliver) {
              gateway::DeliveryEnvelope delivery;
              delivery.set_delivery_id(
                  std::to_string(acceptedText.response.message_id()) + ":" +
                  std::to_string(acceptedText.recipientUid));
              delivery.set_recipient_uid(acceptedText.recipientUid);
              delivery.set_protocol_id(ID_TEXT_SEND_REQ);
              delivery.set_message_id(acceptedText.response.message_id());
              delivery.set_conversation_id(
                  acceptedText.response.conversation_id());
              delivery.set_conversation_seq(
                  acceptedText.response.conversation_seq());
              delivery.set_packet(SerializeTcpPacket(acceptedText.delivery));
              streamService->DeliverToUser(acceptedText.recipientUid,
                                           std::move(delivery));
            }
            return;
          }

          if (command.service_id() == ID_GROUP_TEXT_SEND_REQ) {
            auto acceptedText =
                Service::GetInstance()->Messages().AcceptGroupText(
                    std::move(packet));
            response->set_error(TcpPacketError(acceptedText.response));
            response->set_retryable(acceptedText.response.retryable());
            response->set_packet(SerializeTcpPacket(acceptedText.response));
            streamService->Reply(originGatewayId, originInstanceId,
                                 originReactor, std::move(responseFrame));

            if (acceptedText.shouldDeliver) {
              for (const int64_t recipientUid : acceptedText.recipientUids) {
                gateway::DeliveryEnvelope delivery;
                delivery.set_delivery_id(
                    std::to_string(acceptedText.response.message_id()) + ":" +
                    std::to_string(recipientUid));
                delivery.set_recipient_uid(recipientUid);
                delivery.set_protocol_id(ID_GROUP_TEXT_SEND_REQ);
                delivery.set_message_id(acceptedText.response.message_id());
                delivery.set_conversation_id(
                    acceptedText.response.conversation_id());
                delivery.set_conversation_seq(
                    acceptedText.response.conversation_seq());
                delivery.set_packet(SerializeTcpPacket(acceptedText.delivery));
                streamService->DeliverToUser(recipientUid, std::move(delivery));
              }
            }
            return;
          }

          auto packetResponse = Service::GetInstance()->ExecuteGatewayCommand(
              command.service_id(), command.actor_uid(), std::move(packet));
          response->set_error(TcpPacketError(packetResponse));
          response->set_retryable(isRetryableError(response->error()));
          response->set_packet(SerializeTcpPacket(packetResponse));
          streamService->Reply(originGatewayId, originInstanceId, originReactor,
                               std::move(responseFrame));
        });

    if (!accepted) {
      gateway::MessageToGatewayFrame responseFrame;
      auto *response = responseFrame.mutable_command_result();
      response->set_request_id(requestId);
      response->set_response_service_id(
          __getServiceResponseId(ServiceID(serviceId)));
      response->set_error(ErrorCodes::ResourceExhausted);
      response->set_retryable(true);
      response->set_packet(SerializeTcpPacket(MakeErrorPacket(
          ErrorCodes::ResourceExhausted, "message worker queue is full")));
      Enqueue(std::move(responseFrame));
    }
  }

  void FinishOnce() {
    if (finishing.exchange(true))
      return;
    Finish(grpc::Status::OK);
  }

  GatewayStreamService &service;
  std::string gatewayId;
  std::string instanceId;
  std::string authenticatedPeer;
  uint64_t streamEpoch{0};
  gateway::GatewayToMessageFrame readFrame;
  gateway::MessageToGatewayFrame writeFrame;
  std::mutex writeMutex;
  std::deque<gateway::MessageToGatewayFrame> writeQueue;
  bool writeInFlight{false};
  std::atomic<bool> finishing{false};
  bool registered{false};
};

GatewayStreamService::GatewayStreamService(std::string messageNodeId,
                                           bool requirePeerIdentity)
    : messageNodeId(std::move(messageNodeId)),
      requirePeerIdentity(requirePeerIdentity) {}

grpc::ServerBidiReactor<gateway::GatewayToMessageFrame,
                        gateway::MessageToGatewayFrame> *
GatewayStreamService::Connect(grpc::CallbackServerContext *context) {
  std::string peerIdentity;
  if (auto auth = context->auth_context()) {
    auto identities = auth->GetPeerIdentity();
    if (!identities.empty())
      peerIdentity.assign(identities.front().data(), identities.front().size());
  }
  return new GatewayStreamReactor(*this, std::move(peerIdentity));
}

void GatewayStreamService::Register(const std::string &gatewayId,
                                    const std::string &instanceId,
                                    GatewayStreamReactor *reactor) {
  std::lock_guard<std::mutex> lock(streamsMutex);
  streams[gatewayId] = RegisteredStream{instanceId, reactor};
  LOG_INFO(netLogger, "Gateway stream registered, gateway: {}, instance: {}",
           gatewayId, instanceId);
}

void GatewayStreamService::Unregister(const std::string &gatewayId,
                                      const std::string &instanceId,
                                      GatewayStreamReactor *reactor) {
  std::lock_guard<std::mutex> lock(streamsMutex);
  auto found = streams.find(gatewayId);
  if (found == streams.end() || found->second.instanceId != instanceId ||
      found->second.reactor != reactor)
    return;
  streams.erase(found);
  LOG_INFO(netLogger, "Gateway stream unregistered, gateway: {}, instance: {}",
           gatewayId, instanceId);
}

bool GatewayStreamService::Deliver(const db::SessionLease &lease,
                                   gateway::DeliveryEnvelope envelope) {
  std::lock_guard<std::mutex> lock(streamsMutex);
  auto found = streams.find(lease.gatewayId);
  if (found == streams.end() || found->second.instanceId != lease.instanceId)
    return false;
  gateway::MessageToGatewayFrame frame;
  *frame.mutable_delivery() = std::move(envelope);
  return found->second.reactor->Enqueue(std::move(frame));
}

bool GatewayStreamService::DeliverToUser(int64_t recipientUid,
                                         gateway::DeliveryEnvelope envelope) {
  auto lease = db::RedisDao::GetInstance()->getSessionLease(recipientUid);
  if (lease.empty())
    return false;
  envelope.set_recipient_uid(recipientUid);
  envelope.set_expected_connection_id(lease.connectionId);
  envelope.set_expected_connection_generation(lease.generation);
  if (Deliver(lease, envelope))
    return true;

  auto refreshed = db::RedisDao::GetInstance()->getSessionLease(recipientUid);
  if (refreshed.empty() || (refreshed.gatewayId == lease.gatewayId &&
                            refreshed.instanceId == lease.instanceId &&
                            refreshed.connectionId == lease.connectionId &&
                            refreshed.generation == lease.generation))
    return false;
  envelope.set_expected_connection_id(refreshed.connectionId);
  envelope.set_expected_connection_generation(refreshed.generation);
  return Deliver(refreshed, std::move(envelope));
}

bool GatewayStreamService::Reply(const std::string &gatewayId,
                                 const std::string &instanceId,
                                 GatewayStreamReactor *reactor,
                                 gateway::MessageToGatewayFrame frame) {
  std::lock_guard<std::mutex> lock(streamsMutex);
  auto found = streams.find(gatewayId);
  if (found == streams.end() || found->second.instanceId != instanceId ||
      found->second.reactor != reactor)
    return false;
  return reactor->Enqueue(std::move(frame));
}

void GatewayStreamService::DrainAll(const std::string &reason) {
  draining.store(true, std::memory_order_release);
  std::lock_guard<std::mutex> lock(streamsMutex);
  for (const auto &[_, stream] : streams) {
    gateway::MessageToGatewayFrame frame;
    auto *notice = frame.mutable_drain_notice();
    notice->set_message_node_id(messageNodeId);
    notice->set_reason(reason);
    stream.reactor->Enqueue(std::move(frame));
  }
}

const std::string &GatewayStreamService::MessageNodeId() const {
  return messageNodeId;
}

bool GatewayStreamService::RequirePeerIdentity() const {
  return requirePeerIdentity;
}

bool GatewayStreamService::Draining() const {
  return draining.load(std::memory_order_acquire);
}

}  // namespace wim::rpc
