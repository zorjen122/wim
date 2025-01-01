#pragma once
#include <grpcpp/grpcpp.h>

#include <mutex>

#include "rpc/message.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::GetImServerReq;
using message::GetImServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

class ChatServerNode {
 public:
  ChatServerNode() : host(""), port(""), name(""), con_num(0) {}
  ChatServerNode(const ChatServerNode &cs)
      : host(cs.host), port(cs.port), name(cs.name), con_num(cs.con_num) {}
  ChatServerNode &operator=(const ChatServerNode &cs) {
    if (&cs == this) {
      return *this;
    }

    host = cs.host;
    name = cs.name;
    port = cs.port;
    con_num = cs.con_num;
    return *this;
  }
  std::string host;
  std::string port;
  std::string name;
  int con_num;
};
class StatusServiceImpl final : public StatusService::Service {
 public:
  StatusServiceImpl();
  Status GetImServer(ServerContext *context, const GetImServerReq *request,
                     GetImServerRsp *reply) override;
  Status Login(ServerContext *context, const LoginReq *request,
               LoginRsp *reply) override;

 private:
  void insertToken(int uid, std::string token);
  void loadConfig();

  ChatServerNode allocateChatServer();
  std::unordered_map<std::string, ChatServerNode> _servers;
  std::mutex _server_mtx;
};
