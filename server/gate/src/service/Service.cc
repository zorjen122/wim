#include "Service.h"
#include "StateClient.h"

#include "Const.h"
#include "HttpSession.h"
#include "Logger.h"
#include "Mysql.h"
#include "Redis.h"
#include "spdlog/spdlog.h"
#include "json/value.h"
#include <json/json.h>
#include <yaml-cpp/parser.h>

namespace wim {

Service::Service() {
  using namespace std::placeholders;

  OnGetHandle("/test",
              [this](HttpSession::ResponsePtr response,
                     Json::Value &requestData) -> bool {
                responseWrite(response, "[TEST]");
                return true;
              });

  OnGetHandle("/test-net",
              [this](HttpSession::ResponsePtr response,
                     Json::Value &requestData) -> bool {
                LOG_INFO(businessLogger, "[test_net handle called]");
                auto ret = rpc::StateClient::GetInstance()->TestNetworkPing();
                if (ret.empty()) {
                  responseWrite(response, "[Network is not reachable]");
                } else {
                  responseWrite(response, "[TEST SUCCESS!]");
                }
                return true;
              });

  OnPostHandle("/post-verifycode",
               std::bind(&Service::verifycode, this, _1, _2));
  OnPostHandle("/post-signUp", std::bind(&Service::signUp, this, _1, _2));
  OnPostHandle("/post-forget-password",
               std::bind(&Service::forgetPassword, this, _1, _2));

  OnPostHandle("/post-signIn", std::bind(&Service::signIn, this, _1, _2));
  OnPostHandle("/post-arrhythmia",
               std::bind(&Service::chatArrhythmia, this, _1, _2));
}

void Service::responseWrite(HttpSession::ResponsePtr response,
                            const std::string &data) {
  beast::ostream(response->body()) << data;
}

Json::Value Service::parseRequest(std::shared_ptr<HttpSession> connection) {
  auto buffer = connection->GetRequest()->body().data();
  auto body = boost::beast::buffers_to_string(buffer);

  Json::Reader reader{};
  Json::Value src{};
  bool parseSuccess = reader.parse(body, src);

  return parseSuccess ? src : Json::Value();
}

void Service::OnGetHandle(std::string url, HttpHandler handler) {
  auto ret = getHandlers.insert(make_pair(url, handler));
  if (ret.second == false)
    businessLogger->error("RegisterGet insert is wrong!");
}

void Service::OnPostHandle(std::string url, HttpHandler handler) {
  auto ret = postHandlers.insert(make_pair(url, handler));
  if (ret.second == false)
    businessLogger->error("RegisterPost insert is wrong!");
}

Service::~Service() {}

bool Service::Handle(std::shared_ptr<HttpSession> connection, std::string path,
                     http::verb method) {

  HttpHandler handler;
  bool hasFound = false;
  if (method == http::verb::get) {
    if (getHandlers.find(path) != getHandlers.end()) {
      handler = getHandlers[path];
      hasFound = true;
    }
  } else if (method == http::verb::post) {
    if (postHandlers.find(path) != postHandlers.end()) {
      handler = postHandlers[path];
      hasFound = true;
    }
  }

  auto response = connection->GetResponse();
  if (hasFound == false || handler == nullptr) {
    response->result(http::status::not_found);
    response->set(http::field::content_type, "text/plain");
    responseWrite(response, "url not found");
    return false;
  }

  connection->GetResponse()->set(http::field::content_type, "application/json");
  Json::Value source = parseRequest(connection);

  if (source.empty() && method == http::verb::post) {
    Json::Value rsp;
    rsp["error"] = ErrorCodes::JsonParser;
    responseWrite(response,
                  "content-type not support | " + rsp.toStyledString());
    return true;
  }

  businessLogger->info("[service-handle] path as {}", path);

  response->result(http::status::ok);
  response->set(http::field::server, "GateServer");
  bool handleSuccess = handler(response, source);

  return handleSuccess;
} // namespace wim
bool Service::verifycode(HttpSession::ResponsePtr response,
                         Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  if (!requestData.isMember("email")) {
    businessLogger->info("[post_verifycode-email] Failed to parse JSON data!");

    rspInfo["error"] = ErrorCodes::JsonParser;
    return false;
  }

  auto email = requestData["email"].asString();
  // GetVerifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);

  businessLogger->info("service-post_verifycode] email as {}", email);

  // rspInfo["error"] = rsp.error();
  rspInfo["email"] = requestData["email"];
  return true;
}

bool Service::signUp(HttpSession::ResponsePtr response,
                     Json::Value &requestData) {

  businessLogger->info("[signUp]: start, requestData as {}",
                       requestData.toStyledString());

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto username = requestData["username"].asString();
  auto pwd = requestData["password"].asString();
  auto email = requestData["email"].asString();

  std::string verifycode = "1234";
  // verifycode logic...

  auto uid = db::RedisDao::GetInstance()->generateUserId();

  db::User::Ptr user(new db::User(0, uid, username, pwd, email));
  bool hasUser = db::MysqlDao::GetInstance()->userRegister(user);
  if (hasUser == 1) {
    businessLogger->info("[signUp]: user or email exist");
    rspInfo["error"] = -1;
    return false;
  }
  // verifycode logic...
  businessLogger->info(
      "[signUp]: success! uid as {}, username as {}, password as {}, "
      "email as {}",
      uid, username, pwd, email);
  rspInfo["error"] = 0;
  rspInfo["uid"] = Json::Value::Int64(uid);
  rspInfo["username"] = username;
  rspInfo["password"] = pwd;
  rspInfo["email"] = email;

  return true;
}

bool Service::signIn(HttpSession::ResponsePtr response,
                     Json::Value &requestData) {

  businessLogger->info("[signIn]: start, requestData as {}",
                       requestData.toStyledString());
  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  db::User userInfo;
  auto username = requestData["username"].asString();
  auto password = requestData["password"].asString();

  auto user = wim::db::MysqlDao::GetInstance()->getUser(username);
  if (user == nullptr) {
    businessLogger->error("[password or email input is wrong!]");
    rspInfo["error"] = -1;
    return false;
  }

  auto node = rpc::StateClient::GetInstance()->GetImServer(userInfo.uid);
  if (node.empty()) {
    businessLogger->info("rpc request state service is fialed, user id as {} ",
                         userInfo.uid);
    rspInfo["error"] = -1;
    return false;
  }

  businessLogger->info("[sigIn] user: {} login success", userInfo.username);
  rspInfo["error"] = 0;
  rspInfo["uid"] = (int)userInfo.uid;
  rspInfo["ip"] = node.ip;
  rspInfo["port"] = node.port;

  return true;
}

bool Service::chatArrhythmia(HttpSession::ResponsePtr response,
                             Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto uid = requestData["uid"].asInt();
  // Mysql select...
  rpc::ServerNode node = rpc::StateClient::GetInstance()->GetImServer(uid);
  if (node.ip.empty() || node.port == 0) {
    businessLogger->error("[chat_arrhythmia] get im server failed");
    rspInfo["error"] = ErrorCodes::RPCFailed;
    return false;
  }

  rspInfo["error"] = ErrorCodes::Success;
  rspInfo["host"] = node.ip;
  rspInfo["port"] = node.port;

  return true;
}

bool Service::forgetPassword(HttpSession::ResponsePtr response,
                             Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto username = requestData["username"].asString();
  auto password = requestData["password"].asString();
  auto email = requestData["email"].asString();
  auto verifycode = requestData["verifycode"].asString();

  bool hasVerifycode =
      db::RedisDao::GetInstance()->authVerifycode(email, verifycode);
  if (!hasVerifycode) {
    businessLogger->info(" Verify code is expired or not existed");
    rspInfo["error"] = ErrorCodes::VarifyCodeErr;
    return false;
  }

  auto user = db::MysqlDao::GetInstance()->getUser(username);
  bool updateSuccess = db::MysqlDao::GetInstance()->userModifyPassword(user);
  if (!updateSuccess) {
    businessLogger->error(" update password is failed");
    rspInfo["error"] = ErrorCodes::PasswdUpFailed;
    return false;
  }

  businessLogger->info("success! new password is {}", password);

  rspInfo["error"] = 0;
  rspInfo["email"] = email;
  rspInfo["username"] = username;
  rspInfo["password"] = password;
  rspInfo["verifycode"] = requestData["verifycode"].asString();

  return true;
}
}; // namespace wim