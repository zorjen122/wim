#include "RpcService.h"
#include "Configer.h"
#include "ImActiver.h"
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

// ImRpcService.cpp
grpc::Status ImRpcService::ActiveService(ServerContext *context,
                                         const ActiveRequest *request,
                                         ActiveResponse *response) {
  bool success = ImServiceRunner::GetInstance()->Activate();
  if (!success) {
    response->set_error("failed");
    return grpc::Status::CANCELLED;
  }

  response->set_error("success");
  return grpc::Status::OK;
}
