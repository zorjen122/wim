#pragma once
#include "rpc/message.grpc.pb.h"
#include "rpc/message.pb.h"
#include <grpcpp/grpcpp.h>
#include <mutex>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::AddFriendReq;
using message::AddFriendRsp;
