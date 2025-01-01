
#include "StatusServiceImpl.h"

#include <spdlog/spdlog.h>

#include <climits>

#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "const.h"
#include "json/reader.h"

std::string generate_unique_string() {
  // ����UUID����
  boost::uuids::uuid uuid = boost::uuids::random_generator()();

  // ��UUIDת��Ϊ�ַ���
  std::string unique_string = to_string(uuid);

  return unique_string;
}

StatusServiceImpl::StatusServiceImpl() { loadConfig(); }

void StatusServiceImpl::loadConfig() {
  auto &conf = ConfigMgr::GetInstance();
  auto server_list = conf["ChatServer"]["Name"];

  std::vector<std::string> words;

  std::stringstream ss(server_list);
  std::string word;

  while (std::getline(ss, word, ',')) {
    words.push_back(word);
  }

  for (auto &word : words) {
    if (conf[word]["Name"].empty()) {
      continue;
    }

    ChatServerNode server;
    server.port = conf[word]["Port"];
    server.host = conf[word]["Host"];
    server.name = conf[word]["Name"];
    _servers[server.name] = server;
  }
}

Status StatusServiceImpl::GetImServer(ServerContext *context,
                                      const GetImServerReq *request,
                                      GetImServerRsp *reply) {
  auto server = allocateChatServer();
  auto token = generate_unique_string();
  reply->set_host(server.host);
  reply->set_port(server.port);
  reply->set_error(ErrorCodes::Success);
  reply->set_token(token);
  insertToken(request->uid(), reply->token());

  spdlog::info("[rpc-GetImServer]: return uid{} | (host-{}, port{}, token{})",
               server.host, server.port, token);

  return Status::OK;
}

// ��С������
ChatServerNode StatusServiceImpl::allocateChatServer() {
  std::lock_guard<std::mutex> lock(_server_mtx);
  auto minServer = _servers.begin()->second;
  auto count = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, minServer.name);
  if (count.empty()) {
    // ��������Ĭ������Ϊ���
    minServer.con_num = INT_MAX;
  } else {
    minServer.con_num = std::stoi(count);
  }

  // ʹ�÷�Χ����forѭ��
  for (auto &server : _servers) {
    if (server.second.name == minServer.name) {
      continue;
    }

    auto count = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.second.name);
    if (count.empty()) {
      server.second.con_num = INT_MAX;
    } else {
      server.second.con_num = std::stoi(count);
    }

    if (server.second.con_num < minServer.con_num) {
      minServer = server.second;
    }
  }

  return minServer;
}

Status StatusServiceImpl::Login(ServerContext *context, const LoginReq *request,
                                LoginRsp *reply) {
  auto uid = request->uid();
  auto token = request->token();

  std::string uid_str = std::to_string(uid);
  std::string token_key = USERTOKENPREFIX + uid_str;
  std::string token_value = "";
  bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
  if (success) {
    reply->set_error(ErrorCodes::UidInvalid);
    return Status::OK;
  }

  if (token_value != token) {
    reply->set_error(ErrorCodes::TokenInvalid);
    return Status::OK;
  }
  reply->set_error(ErrorCodes::Success);
  reply->set_uid(uid);
  reply->set_token(token);
  return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token) {
  std::string uid_str = std::to_string(uid);
  std::string token_key = USERTOKENPREFIX + uid_str;
  RedisMgr::GetInstance()->Set(token_key, token);
}
