#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
  long uid = 123456;
  std::string filename = "test.txt";
  std::string fileChunk = "Hello, world!";
  fs::path saveDir = fs::path("./") / std::to_string(uid);
  fs::path saveFilePath = saveDir / filename;

  try {
    if (!fs::exists(saveDir)) {
      fs::create_directories(saveDir); // 递归创建所有不存在的目录
    }

    std::ofstream ofs(saveFilePath, std::ios::binary | std::ios::app);
    if (!ofs.is_open()) {
      throw std::runtime_error("Failed to open file: " + saveFilePath.string());
    }
    ofs.write(fileChunk.data(), fileChunk.size());
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
};