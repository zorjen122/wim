#include "Configer.h"
#include "Service.h"
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <thread>

namespace wim::rpc::test {

class FileClientTest : public ::testing::Test {
protected:
  void SetUp() override {
    serverThread = std::thread([this]() {
      server = std::make_unique<FileServer>(4);
      server->Run(50051);
    });
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 等待服务器启动

    auto channel = grpc::CreateChannel("localhost:50051",
                                       grpc::InsecureChannelCredentials());
    stub = file::FileService::NewStub(channel);
  }

  void TearDown() override {
    server->Stop();
    serverThread.join();
  }

  std::unique_ptr<FileServer> server;
  std::thread serverThread;
  std::unique_ptr<file::FileService::Stub> stub;
};

TEST_F(FileClientTest, UploadSmallFile) {
  grpc::ClientContext context;
  file::UploadRequest request;
  file::UploadResponse response;

  auto *chunk = request.mutable_chunk();
  chunk->set_filename("test.txt");
  chunk->set_data("Hello, gRPC!");
  request.set_user_id(123);

  grpc::Status status = stub->Upload(&context, request, &response);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(response.status(), file::TransferStatus::SUCCESS);

  // 验证文件是否创建
  std::ifstream file(Configer::getSaveFilePath() + "/123/test.txt");
  EXPECT_TRUE(file.good());

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  EXPECT_EQ(content, "Hello, gRPC!");
}

TEST_F(FileClientTest, UploadLargeFileInChunks) {
  // 测试分块上传逻辑
  const std::string testData(1024 * 1024, 'A'); // 1MB数据

  grpc::ClientContext context;
  file::UploadRequest request;
  file::UploadResponse response;

  auto *chunk = request.mutable_chunk();
  chunk->set_filename("large.bin");
  chunk->set_data(testData);
  request.set_user_id(123);

  grpc::Status status = stub->Upload(&context, request, &response);
  EXPECT_TRUE(status.ok());

  // 验证文件大小
  std::ifstream file(Configer::getSaveFilePath() + "/123/large.bin",
                     std::ios::binary | std::ios::ate);
  EXPECT_EQ(file.tellg(), testData.size());
}

} // namespace wim::rpc::test