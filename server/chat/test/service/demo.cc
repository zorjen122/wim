#include <boost/asio/io_context.hpp>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

#include "base.h"

void foo() {

  std::string host = "\"127.0.0.1\"\\n";

  auto first = host.find_first_of("\"");
  auto last = host.find_last_of("\"");

  std::cout << first << "\t" << last << "\n";
  auto s = host.substr(first + 1, last - 1);

  std::cout << "normal: " << host << "\n";
  std::cout << "change after: " << s << "\n";

  short p = 10, p2 = 20;
  char buf[1024]{};

  memcpy(buf, &p, sizeof(short));
  memcpy(&buf[2], &p2, sizeof(short));
  std::cout << "buf: " << *(short *)buf << "\n";
  std::cout << "buf: " << *(short *)&buf[2] << "\n";
}

int main() {

  std::unordered_map<std::string, std::string> headers;
  headers["Authorization"] =
      std::string("Bearer ") + "sk-28a5d2005ac848fcb41f28c55c902e2e";
  std::string host = "api.deepseek.com";
  std::string port = "80";
  std::string path = "/chat/completions";
  Json::Value msg1;
  msg1["role"] = "system", msg1["content"] = "You are a helpful assistant.";
  Json::Value msg2;
  msg2["role"] = "user",
  msg2["content"] =
      "Hello!, plases you tanslate into chinese language for apple.";

  Json::Value data;
  data["model"] = "deepseek-chat";
  data["message"].append(msg1);
  data["message"].append(msg2);

  auto rsp = base::post(host, port, path, data.toStyledString(), headers);
  if (rsp.empty()) {
    spdlog::error("failed to send request");
    return -1;
  }

  spdlog::info("[response: {}]", rsp);

  return 0;
}