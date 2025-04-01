#pragma once
#include "Const.h"
#include "RpcPool.h"
#include "im.grpc.pb.h"
#include "im.pb.h"
#include <grpc/grpc.h>

using im::ActiveRequest;
using im::ActiveResponse;
using im::ImService;
class ImRpc : public Singleton<ImRpc> {
public:
  ImRpc();
  bool ActiveService();

private:
  std::unique_ptr<RpcPool<ImService>> pool = nullptr;
};