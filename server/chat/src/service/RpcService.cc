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

std::unique_ptr<grpc::Server> ImRpcService::CreateImRpcServer() {

  auto conf = Configer::getConfig("server");

  if (conf.IsNull())
    spdlog::error("Configer::getConfig(\"Server\") failed");

  auto host = conf["self"]["host"].as<std::string>();
  auto rpcPort = conf["self"]["rpcPort"].as<std::string>();
  auto address = host + ":" + rpcPort;

  ImRpcService service;
  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  return server;
}
