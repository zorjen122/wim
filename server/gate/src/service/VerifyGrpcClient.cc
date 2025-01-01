#include "VerifyGrpcClient.h"

#include "Const.h"

VerifyGrpcClient::VerifyGrpcClient() {
  auto &gCfgMgr = Configer::GetInstance();
  std::string host = gCfgMgr["VerifyServer"]["Host"];
  std::string port = gCfgMgr["VerifyServer"]["Port"];
  pool_.reset(new RPConPool(5, host, port));
}