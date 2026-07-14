#include "imRpc.h"
#include "Configer.h"
#include "Const.h"
#include "Metrics.h"
#include "RequestContext.h"
#include "RpcPool.h"
#include "global.h"
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
namespace wim::rpc {
ImRpc::ImRpc(ImNode::ptr node, size_t poolSize) {
  auto conf = Configer::getNode("server");

  pool.reset(
      new RpcPool<ImService>(poolSize, node->getIp(), node->getRpcPort()));
  spdlog::info("ImRpc initialized with pool size {}", pool->getPoolSize());
}

bool ImRpc::ActiveService() {
  ActiveRequest req;
  ActiveResponse rsp;
  grpc::ClientContext context;
  // 故障激活属于控制面操作，同样必须有明确截止时间并归还池中 stub。
  auto deadline = RequestContextScope::CurrentDeadlineOr(
      std::chrono::milliseconds(1000));
  auto remaining = std::max(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - RequestContext::Clock::now()),
      std::chrono::milliseconds(0));
  context.set_deadline(std::chrono::system_clock::now() + remaining);
  auto caller = pool->getConnection();
  if (caller == nullptr) {
    rsp.set_error("No available connection");
    spdlog::warn("No available connection");
    return false;
  }
  Defer defer([this, &caller]() {
    pool->returnConnection(std::move(caller));
  });
  auto status = caller->ActiveService(&context, req, &rsp);
  if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    Metrics::Increment(Metric::RpcDeadlineExceeded);
    spdlog::warn("State激活Chat RPC超时");
  }
  if (!status.ok()) {
    spdlog::warn("ActiveService failed: {}", status.error_message());
    return false;
  }
  spdlog::info("ActiveService success");
  return true;
}

size_t ImRpc::getPoolSize() const {
  return pool->getPoolSize();
}
};  // namespace wim::rpc
