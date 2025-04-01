#include "imRpc.h"
#include "Configer.h"
#include "RpcPool.h"
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>
ImRpc::ImRpc() {

  auto conf = Configer::getConfig("server");

  if (conf.IsNull())
    spdlog::error("Configer::getConfig(\"Server\") failed");

  auto imBackup = conf["imBackup"];
  auto im_1 = imBackup["im-1"];
  auto im_1_host = im_1["host"].as<std::string>();
  auto im_1_port = im_1["port"].as<std::string>();
  auto im_1_rpcCount = im_1["rpcCount"].as<int>();

  pool.reset(new RpcPool<ImService>(im_1_rpcCount, im_1_host, im_1_port));
}

bool ImRpc::ActiveService() {
  ActiveRequest req;
  ActiveResponse rsp;
  grpc::ClientContext context;
  auto caller = pool->getConnection();
  if (caller == nullptr) {
    rsp.set_error("No available connection");
    return false;
  }
  auto status = caller->ActiveService(&context, req, &rsp);
  if (!status.ok()) {
    spdlog::warn("ActiveService failed: {}", status.error_message());
    return false;
  }
  return true;
}