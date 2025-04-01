#include "Service.h"

#include "Const.h"
#include "HttpSession.h"
#include "MysqlOperator.h"
#include "RedisOperator.h"
#include "StateClient.h"
#include "spdlog/spdlog.h"
#include "json/value.h"
#include <json/json.h>
#include <yaml-cpp/parser.h>

Service::Service() {
  using namespace std::placeholders;

  OnGetHandle("/test",
              [this](HttpSession::ResponsePtr response,
                     Json::Value &requestData) -> bool {
                responseWrite(response, "[TEST]");
                return true;
              });

  OnPostHandle("/post-verifycode",
               std::bind(&Service::verifycodeHandle, this, _1, _2));
  OnPostHandle("/post-register",
               std::bind(&Service::registerHandle, this, _1, _2));
  OnPostHandle("/post-reset", std::bind(&Service::resetHandle, this, _1, _2));

  OnPostHandle("/post-login", std::bind(&Service::loginHandle, this, _1, _2));
  OnPostHandle("/post-arrhythmia",
               std::bind(&Service::chatArrhythmiaHandle, this, _1, _2));
}

void Service::responseWrite(HttpSession::ResponsePtr response,
                            const std::string &data) {
  beast::ostream(response->body()) << data;
}

Json::Value Service::parseRequest(std::shared_ptr<HttpSession> connection) {
  auto buffer = connection->GetRequest()->body().data();
  auto body = boost::beast::buffers_to_string(buffer);

  connection->GetRequest()->set(http::field::content_type, "text/json");
  Json::Reader reader;
  Json::Value src;
  reader.parse(body, src);

  return src;
}

void Service::OnGetHandle(std::string url, HttpHandler handler) {
  auto ret = getHandlers.insert(make_pair(url, handler));
  if (ret.second == false)
    spdlog::error("RegisterGet insert is wrong!");
}

void Service::OnPostHandle(std::string url, HttpHandler handler) {
  auto ret = postHandlers.insert(make_pair(url, handler));
  if (ret.second == false)
    spdlog::error("RegisterPost insert is wrong!");
}

Service::~Service() {}

bool Service::Handle(std::shared_ptr<HttpSession> connection, std::string path,
                     http::verb method) {

  HttpHandler handler;
  if (method == http::verb::get) {
    if (getHandlers.find(path) == getHandlers.end()) {
      return false;
    }
    handler = getHandlers[path];
  } else if (method == http::verb::post) {
    if (postHandlers.find(path) == postHandlers.end()) {
      return false;
    }
    handler = postHandlers[path];
  }

  if (handler == nullptr)
    return false;

  Json::Value source = parseRequest(connection);
  auto response = connection->GetResponse();
  connection->GetResponse()->set(http::field::content_type, "text/json");

  if (source.empty()) {
    Json::Value rsp;
    rsp["error"] = ErrorCodes::JsonParser;
    beast::ostream(response->body()) << rsp.toStyledString();
    return false;
  }

  bool handleSuccess = handler(response, source);

  return handleSuccess;
}

bool Service::verifycodeHandle(HttpSession::ResponsePtr response,
                               Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  if (!requestData.isMember("email")) {
    spdlog::info("[post_verifycode-email] Failed to parse JSON data!");

    rspInfo["error"] = ErrorCodes::JsonParser;
    return false;
  }

  auto email = requestData["email"].asString();
  // GetVerifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);

  spdlog::info("service-post_verifycode] email as {}", email);

  // rspInfo["error"] = rsp.error();
  rspInfo["email"] = requestData["email"];
  return true;
}

bool Service::registerHandle(HttpSession::ResponsePtr response,
                             Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto email = requestData["email"].asString();
  auto name = requestData["user"].asString();
  auto pwd = requestData["password"].asString();
  auto confirm = requestData["confirm"].asString();
  auto icon = requestData["icon"].asString();

  if (pwd != confirm) {
    spdlog::info("[post_register] password is wrong");
    rspInfo["error"] = ErrorCodes::PasswdErr;
    return true;
  }

  std::string verifycode;
  // verifycode logic...

  int uid = MysqlOperator::GetInstance()->RegisterUser(name, email, pwd);
  if (uid == 0 || uid == -1) {
    spdlog::info("[post_register] user or email exist");
    rspInfo["error"] = -1;
    return true;
  }

  rspInfo["error"] = 0;
  rspInfo["uid"] = uid;
  rspInfo["email"] = email;
  rspInfo["user"] = name;
  rspInfo["password"] = pwd;
  rspInfo["confirm"] = confirm;
  rspInfo["icon"] = icon;
  rspInfo["verifycode"] = requestData["verifycode"].asString();

  return true;
}

bool Service::resetHandle(HttpSession::ResponsePtr response,
                          Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto email = requestData["email"].asString();
  auto name = requestData["user"].asString();
  auto pwd = requestData["password"].asString();

  std::string verifycode;
  bool has_verifycode = RedisOperator::GetInstance()->Get(
      requestData["email"].asString(), verifycode);
  if (!has_verifycode) {
    spdlog::info("[post-reset] Verify code is expired");
    rspInfo["error"] = ErrorCodes::VarifyCodeErr;
    return false;
  }

  if (verifycode != requestData["verifycode"].asString()) {
    spdlog::info("[post-reset] verifycode is invalid");

    rspInfo["error"] = ErrorCodes::VarifyCodeErr;
    return false;
  }
  bool email_valid = MysqlOperator::GetInstance()->CheckEmail(name, email);
  if (!email_valid) {
    spdlog::info("[post-reset] email input is invalid");
    rspInfo["error"] = ErrorCodes::EmailNotMatch;
    return false;
  }

  bool update_success = MysqlOperator::GetInstance()->UpdatePassword(name, pwd);
  if (!update_success) {
    spdlog::error(" update password is failed");
    rspInfo["error"] = ErrorCodes::PasswdUpFailed;
    return false;
  }

  spdlog::info("[post-reset] success! new password is {}", pwd);

  rspInfo["error"] = 0;
  rspInfo["email"] = email;
  rspInfo["user"] = name;
  rspInfo["password"] = pwd;
  rspInfo["verifycode"] = requestData["verifycode"].asString();

  return true;
}
bool Service::loginHandle(HttpSession::ResponsePtr response,
                          Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  UserInfo userInfo;
  auto email = requestData["email"].asString();
  auto pwd = requestData["password"].asString();
  spdlog::info("debug: email: {}, pwd: {} ", email, pwd);

  bool userIsExist =
      MysqlOperator::GetInstance()->CheckUserExist(email, pwd, userInfo);
  if (!userIsExist) {
    spdlog::error("[post_login: password or email input is wrong!]");
    rspInfo["error"] = ErrorCodes::PasswdInvalid;
    return false;
  }

  // auto reply = StatusGrpcClient::GetInstance()->GetImServer(userInfo.uid);
  // if (reply.error()) {
  //   spdlog::error("[grpc about gateway im server is failed, error as {}",
  //                 reply.error());
  //   rspInfo["error"] = ErrorCodes::RPCFailed;
  //   return false;
  // }

  // spdlog::info("[post-login] success!, user id as {} ", userInfo.uid);
  // rspInfo["error"] = 0;
  // rspInfo["email"] = email;
  // rspInfo["uid"] = userInfo.uid;
  // rspInfo["token"] = reply.token();
  // rspInfo["host"] = reply.host();
  // rspInfo["port"] = reply.port();
  // rsp_package = rspInfo.toStyledString();

  return true;
}

bool Service::chatArrhythmiaHandle(HttpSession::ResponsePtr response,
                                   Json::Value &requestData) {

  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto uid = requestData["uid"].asInt();
  // Mysql select...
  ServerNode node = StateClient::GetInstance()->GetImServer(uid);
  if (node.ip.empty() || node.port == 0) {
    spdlog::error("[chat_arrhythmia] get im server failed");
    rspInfo["error"] = ErrorCodes::RPCFailed;
    return false;
  }

  rspInfo["error"] = ErrorCodes::Success;
  rspInfo["host"] = node.ip;
  rspInfo["port"] = node.port;

  return true;
}