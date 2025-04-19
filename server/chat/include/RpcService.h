#pragma once
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

namespace wim::rpc {

using grpc::ServerBuilder;
using grpc::ServerContext;
using im::ActiveRequest;
using im::ActiveResponse;
using im::ImService;

using im::NotifyAddFriendRequest;
using im::NotifyAddFriendResponse;

using im::ReplyAddFriendRequest;
using im::ReplyAddFriendResponse;

using im::TextSendMessageRequest;
using im::TextSendMessageResponse;

class ImRpcService final : public ImService::Service {
public:
  grpc::Status ActiveService(ServerContext *context,
                             const ActiveRequest *request,
                             ActiveResponse *response) override;

  grpc::Status NotifyAddFriend(ServerContext *context,
                               const NotifyAddFriendRequest *request,
                               NotifyAddFriendResponse *response) override;
  grpc::Status ReplyAddFriend(ServerContext *context,
                              const ReplyAddFriendRequest *request,
                              ReplyAddFriendResponse *response) override;

  grpc::Status TextSendMessage(ServerContext *context,
                               const TextSendMessageRequest *request,
                               TextSendMessageResponse *response) override;
};

}; // namespace wim::rpc