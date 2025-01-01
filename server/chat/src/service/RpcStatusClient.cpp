#include "RpcStatusClient.h"

GetChatSessionRsp StatusGrpcClient::GetChatSession(int uid) {
  ClientContext context;
  GetChatSessionRsp reply;
  GetChatSessionReq request;
  request.set_uid(uid);
  auto stub = pool_->getConnection();
  Status status = stub->GetChatSession(&context, request, &reply);
  Defer defer([&stub, this]() { pool_->returnConnection(std::move(stub)); });
  if (status.ok()) {
    return reply;
  } else {
    reply.set_error(ErrorCodes::RPCFailed);
    return reply;
  }
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token) {
  ClientContext context;
  LoginRsp reply;
  LoginReq request;
  request.set_uid(uid);
  request.set_token(token);

  auto stub = pool_->getConnection();
  Status status = stub->Login(&context, request, &reply);
  Defer defer([&stub, this]() { pool_->returnConnection(std::move(stub)); });
  if (status.ok()) {
    return reply;
  } else {
    reply.set_error(ErrorCodes::RPCFailed);
    return reply;
  }
}

StatusGrpcClient::StatusGrpcClient() {
  auto &gCfgMgr = Configer::GetInstance();
  std::string host = gCfgMgr["StatusServer"]["Host"];
  std::string port = gCfgMgr["StatusServer"]["Port"];
  pool_.reset(new StatusConPool(5, host, port));
}
