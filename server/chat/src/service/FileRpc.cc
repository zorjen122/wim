#include "FileRpc.h"
#include "Configer.h"
#include "Logger.h"
#include "RpcPool.h"
#include <exception>
#include <grpcpp/client_context.h>
#include <string>
namespace wim::rpc {
FileRpc::FileRpc() {
  try {
    auto conf = Configer::getNode("server");
    std::string host = conf["file"]["host"].as<std::string>();
    unsigned short rpcPort = conf["file"]["rpcPort"].as<int>();
    int rpcCount = conf["file"]["rpcCount"].as<int>();
    pool.reset(
        new RpcPool<FileService>(rpcCount, host, std::to_string(rpcPort)));
    LOG_INFO(netLogger,
             "FileRpc::FileRpc() | host: {}, rpcPort: {}, rpcCount: {}", host,
             rpcPort, pool->getPoolSize());
  } catch (std::exception &e) {
    LOG_ERROR(netLogger, "FileRpc::FileRpc() is wrong | what: {}", e.what());
  }
}
FileRpc::~FileRpc() {

  LOG_INFO(netLogger, "FileRpc::~FileRpc() | use count: {}");
}
grpc::Status FileRpc::forwardUpload(const UploadRequest &req,
                                    UploadResponse &resp) {
  ClientContext context;
  auto rpc = pool->getConnection();
  if (rpc == nullptr) {
    LOG_INFO(netLogger, "FileRpc::forwardUpload() | No available connection");
    return grpc::Status(grpc::StatusCode::INTERNAL, "No available connection");
  }
  Defer defer([&]() { pool->returnConnection(std::move(rpc)); });

  return rpc->Upload(&context, req, &resp);
}

auto FileRpc::getPoolSize() const { return pool->getPoolSize(); }
}; // namespace wim::rpc