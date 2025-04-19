#pragma once
#include "RpcPool.h"
#include "global.h"
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpc/grpc.h>

using im::ActiveRequest;
using im::ActiveResponse;
using im::ImService;
namespace wim::rpc {
class ImRpc {
public:
  ImRpc(ImNode::ptr node, size_t poolSize);
  bool ActiveService();
  size_t getPoolSize() const;

private:
  std::unique_ptr<RpcPool<ImService>> pool = nullptr;
};
}; // namespace wim::rpc