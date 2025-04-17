#pragma once
#include <memory>
#include <string>
#include <vector>

class ImNode {
public:
  using ptr = std::shared_ptr<ImNode>;
  ImNode(const std::string &ip, const std::string &port,
         const std::string &status)
      : ip(ip), port(port), status(status) {}
  bool empty() const { return ip.empty() || port.empty(); }
  std::string getIp() const { return ip; }
  std::string getPort() const { return port; }
  std::string getStatus() const { return status; }
  void setStatus(const std::string &status) { this->status = status; }
  void appendConnection(long connection) { connections.push_back(connection); }

private:
  std::string ip;
  std::string port;
  std::string status;
  std::vector<long> connections;
};