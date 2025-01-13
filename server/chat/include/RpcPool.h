#pragma once
#include <grpcpp/grpcpp.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

#include <atomic>
#include <condition_variable>
#include <queue>
#include <unordered_map>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

template <class RPC> class RpcPool {
public:
  RpcPool(size_t poolSize, std::string host, std::string port)
      : size(poolSize), isStop(false) {
    for (size_t i = 0; i < poolSize; ++i) {
      std::shared_ptr<Channel> channel = grpc::CreateChannel(
          host + ":" + port, grpc::InsecureChannelCredentials());
      conGroup.push(typename RPC::NewStub(channel));
    }
  }

  ~RpcPool() {
    std::lock_guard<std::mutex> lock(queueMutex);
    Close();
    while (!conGroup.empty()) {
      conGroup.pop();
    }
  }

  std::unique_ptr<typename RPC::Stub> getConnection() {
    std::unique_lock<std::mutex> lock(queueMutex);
    cond.wait(lock, [this] {
      if (isStop) {
        return true;
      }
      return !conGroup.empty();
    });
    if (isStop) {
      return nullptr;
    }
    auto con = std::move(conGroup.front());
    conGroup.pop();
    return con;
  }

  void returnConnection(std::unique_ptr<typename RPC::Stub> context) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (isStop) {
      return;
    }
    conGroup.push(std::move(context));
    cond.notify_one();
  }

  void Close() {
    isStop = true;
    cond.notify_all();
  }

private:
  std::atomic<bool> isStop;
  size_t size;
  std::queue<std::unique_ptr<typename RPC::Stub>> conGroup;
  std::mutex queueMutex;
  std::condition_variable cond;
};
