#pragma once
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
using grpc::ServerBuilder;
using grpc::ServerContext;
using im::ActiveRequest;
using im::ActiveResponse;
using im::ImService;
class ImRpcService final : public ImService::Service {
public:
  grpc::Status ActiveService(ServerContext *context,
                             const ActiveRequest *request,
                             ActiveResponse *response);
};
