#pragma once
#include <grpcpp/grpcpp.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

#include <atomic>
#include <condition_variable>
#include <queue>
#include <unordered_map>

#include "Configer.h"
#include "Const.h"
#include "Singleton.h"
#include "data.h"
#include "rpc/message.grpc.pb.h"
#include "rpc/message.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::ChatService;
using message::GetChatSessionRsp;
using message::LoginReq;
using message::LoginRsp;

using message::TextChatData;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;

class rpcConnectPool {
 public:
  rpcConnectPool(size_t poolSize, std::string host, std::string port)
      : poolSize_(poolSize), host_(host), port_(port), isStop_(false) {
    for (size_t i = 0; i < poolSize_; ++i) {
      std::shared_ptr<Channel> channel = grpc::CreateChannel(
          host + ":" + port, grpc::InsecureChannelCredentials());

      connections_.push(ChatService::NewStub(channel));
    }
  }

  ~rpcConnectPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    Close();
    while (!connections_.empty()) {
      connections_.pop();
    }
  }

  std::unique_ptr<ChatService::Stub> getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] {
      if (isStop_) {
        return true;
      }
      return !connections_.empty();
    });
    // 如果停止则直接返回空指针
    if (isStop_) {
      return nullptr;
    }
    auto context = std::move(connections_.front());
    connections_.pop();
    return context;
  }

  void returnConnection(std::unique_ptr<ChatService::Stub> context) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isStop_) {
      return;
    }
    connections_.push(std::move(context));
    cond_.notify_one();
  }

  void Close() {
    isStop_ = true;
    cond_.notify_all();
  }

 private:
  std::atomic<bool> isStop_;
  size_t poolSize_;
  std::string host_;
  std::string port_;
  std::queue<std::unique_ptr<ChatService::Stub>> connections_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

class ChatGrpcClient : public Singleton<ChatGrpcClient> {
  friend class Singleton<ChatGrpcClient>;

 public:
  ~ChatGrpcClient() {}

  // 通知对方的好友申请
  AddFriendRsp NotifyAddFriend(std::string ip, const AddFriendReq &req);

  // 同意对方的好友申请
  AuthFriendRsp NotifyAuthFriend(std::string ip, const AuthFriendReq &req);
  bool GetBaseInfo(std::string base_key, int uid,
                   std::shared_ptr<UserInfo> &userinfo);

  // 推送文本消息
  TextChatMsgRsp NotifyTextChatMsg(std::string ip, const TextChatMsgReq &req,
                                   const Json::Value &rtvalue);

 private:
  ChatGrpcClient();
  std::unordered_map<std::string, std::unique_ptr<rpcConnectPool>> _pools;
};
