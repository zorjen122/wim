#include "imRpc.h"
#include "Configer.h"
#include "RpcPool.h"
#include "global.h"
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>
namespace wim::rpc {
ImRpc::ImRpc(ImNode::ptr node, size_t poolSize) {

  auto conf = Configer::getConfig("server");

  pool.reset(
      new RpcPool<ImService>(poolSize, node->getIp(), node->getRpcPort()));
  spdlog::info("ImRpc initialized with pool size {}", pool->getPoolSize());
}

bool ImRpc::ActiveService() {
  ActiveRequest req;
  ActiveResponse rsp;
  grpc::ClientContext context;
  auto caller = pool->getConnection();
  if (caller == nullptr) {
    rsp.set_error("No available connection");
    spdlog::warn("No available connection");
    return false;
  }
  auto status = caller->ActiveService(&context, req, &rsp);
  if (!status.ok()) {
    spdlog::warn("ActiveService failed: {}", status.error_message());
    return false;
  }
  spdlog::info("ActiveService success");
  return true;
}

size_t ImRpc::getPoolSize() const { return pool->getPoolSize(); }
}; // namespace wim::rpc