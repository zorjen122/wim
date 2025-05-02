#include "Service.h"
#include "Configer.h"
#include "Logger.h"
#include "file.pb.h"
#include <fstream>
#include <grpcpp/support/status.h>
#include <string>

namespace wim::rpc {

FileWorker::FileWorker() {
  workThread = std::thread([this]() {
    while (!stopEnable) {
      std::unique_lock<std::mutex> lock(executeMutex);
      cond.wait(lock, [this]() {
        if (stopEnable) {
          return true;
        }
        if (tasks.empty()) {
          return false;
        }
        return true;
      });
      if (stopEnable) {
        break;
      }
      auto task = tasks.front();
      task->Process();
      tasks.pop();
    }
  });
}

FileWorker::~FileWorker() {
  stopEnable = true;
  cond.notify_one();

  if (workThread.joinable())
    workThread.join();
}

void FileWorker::PushTask(Task task) {
  {
    std::lock_guard<std::mutex> lock(executeMutex);
    tasks.push(task);
  }
  cond.notify_one();
}
FileServiceImpl::FileServiceImpl(int workerSize) {
  for (int i = 0; i < workerSize; i++) {
    workers.emplace_back(std::make_unique<FileWorker>());
  }
}
FileServiceImpl::~FileServiceImpl() {
  for (auto &worker : workers)
    worker.reset();
}

std::string FileServiceImpl::getFileType(const std::string &filename) {
  // get .xxx
  std::string suffix = filename.substr(filename.rfind(".") + 1);
  return suffix;
}

void FileServiceImpl::PushTask(FileWorker::Task task){
  static int routeCount = 0;
  workers[routeCount]->PushTask(task);
routeCount++;
if(routeCount >= workers.size())
routeCount = 0;
}

grpc::Status FileServiceImpl::Upload(grpc::ServerContext *context,
                                     const file::UploadRequest *request,
                                     file::UploadResponse *response) {

  auto handle = [&] {
    long uid = request->user_id();
    auto fileChunk = request->chunk();
    std::string filename = fileChunk.filename();
    std::string saveFilePath = Configer::getSaveFilePath() + "/" +
                               std::to_string(uid) + "/" + filename;

    // 模式： 追加
    std::ofstream ofs(saveFilePath, std::ios::binary | std::ios::app);
    ofs.write(fileChunk.data().c_str(), fileChunk.data().size());
  };

  auto task = std::make_shared<
      UnifiedCallData<file::UploadRequest, file::UploadResponse>>(
      context, request, response, handle);

  PushTask(task);

  return grpc::Status::OK;
}

grpc::Status FileServiceImpl::Send(grpc::ServerContext *context,
                                   const file::SendRequest *request,
                                   file::SendResponse *response) {
  auto handle = [&]() {
    // 待消息划分为一个服务时，实现该类似函数；
  };

  auto task =
      std::make_shared<UnifiedCallData<file::SendRequest, file::SendResponse>>(
          context, request, response, handle);
          PushTask(task);

  return grpc::Status::OK;
}

FileServer::FileServer(size_t workerSize)
    : workerSize(workerSize), stopEnable(true) {
  if (workerSize > std::thread::hardware_concurrency()) {
    LOG_ERROR(
        netLogger,
        "workerSize is too large, it will be set to hardware_concurrency");
    workerSize = std::thread::hardware_concurrency();
  }
}

FileServer::~FileServer() {}

void FileServer::Run(unsigned short port) {

  std::string serverAddress = "0.0.0.0:" + std::to_string(port);
  ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:" + std::to_string(port),
                           grpc::InsecureServerCredentials());

  service.reset(new FileServiceImpl(workerSize));
  builder.RegisterService(service.get());
  server = builder.BuildAndStart();

  LOG_INFO(netLogger, "Server listening on " + serverAddress);

  stopEnable = false;
  server->Wait();
}

void FileServer::Stop() {
  if (server) {
    server->Shutdown();
  }
  if (service) {
    service.reset();
  }
}
bool FileServer::stopped() const { return stopEnable.load(); }

}; // namespace wim::rpc