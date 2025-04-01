#include "service.h"
#include "Configer.h"
#include "im.pb.h"
#include "imRpc.h"
#include <grpcpp/support/status.h>

grpc::Status StateServiceImpl::GetImServer(grpc::ServerContext *context,
                                           const ConnectUser *request,
                                           ConnectUserRsp *response) {

  auto conf = Configer::getConfig("server");

  if (conf.IsNull())
    spdlog::error("Configer::getConfig(\"Server\") failed");

  auto backupIM = conf["imBackup"]["im-1"];
  auto im1Host = backupIM["host"].as<std::string>();
  auto im1Port = backupIM["port"].as<std::string>();

  static std::map<int, std::vector<int>> imInfo;
  static std::vector<int> backupImID = {1};
  static std::map<int, std::pair<std::string, std::string>> ImNode = {
      {1, {im1Host, im1Port}}};
  static std::map<int, bool> imState;

  int uid = request->id();
  int idx = 0;

  if (imState.find(backupImID[idx]) == imState.end()) {
    imState[backupImID[idx]] = true;
    bool onActive = ImRpc::GetInstance()->ActiveService();
    if (!onActive) {
      response->set_ip("");
      response->set_port(0);
      return grpc::Status(grpc::StatusCode::INTERNAL, "ImRpc active failed");
    }
  }

  imInfo[backupImID[idx]].push_back(uid);
  response->set_ip(ImNode[backupImID[idx]].first);
  response->set_port(atoi(ImNode[backupImID[idx]].second.c_str()));
  return grpc::Status::OK;
}