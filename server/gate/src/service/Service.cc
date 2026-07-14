#include "Service.h"
#include "StateClient.h"

#include "Const.h"
#include "HttpSession.h"
#include "Logger.h"
#include "Mysql.h"
#include "Metrics.h"
#include "Redis.h"
#include "RequestContext.h"
#include "message.grpc.pb.h"
#include "message.pb.h"
#include "spdlog/spdlog.h"
#include <grpcpp/grpcpp.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <yaml-cpp/parser.h>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <algorithm>
#include <chrono>

namespace wim {
namespace {
// Gate 生成短期随机凭证，仅用于把已认证 uid 安全交接给 Chat。
std::string GenerateChatAuthToken() {
  boost::uuids::random_generator generator;
  return boost::uuids::to_string(generator()) +
         boost::uuids::to_string(generator());
}

constexpr long kChatAuthTokenTtlSeconds = 15 * 60;

int requestVerifyCode(const std::string &email) {
  auto config = Configer::getNode("server");
  if (!config || !config["verifyRPC"]) {
    businessLogger->error("[post_verifycode] verifyRPC config not found");
    return ErrorCodes::RPCFailed;
  }

  const auto host = config["verifyRPC"]["host"].as<std::string>();
  const auto port = config["verifyRPC"]["port"].as<std::string>();
  auto channel = grpc::CreateChannel(host + ":" + port,
                                     grpc::InsecureChannelCredentials());
  auto stub = message::VarifyService::NewStub(channel);

  message::GetVarifyReq req;
  req.set_email(email);
  message::GetVarifyRsp rsp;
  grpc::ClientContext context;
  auto deadline = RequestContextScope::CurrentDeadlineOr(
      std::chrono::milliseconds(1000));
  auto remaining = std::max(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - RequestContext::Clock::now()),
      std::chrono::milliseconds(0));
  context.set_deadline(std::chrono::system_clock::now() + remaining);
  auto status = stub->GetVarifyCode(&context, req, &rsp);
  if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    Metrics::Increment(Metric::RpcDeadlineExceeded);
  }
  if (!status.ok()) {
    businessLogger->error("[post_verifycode] verify rpc failed: {}",
                          status.error_message());
    return ErrorCodes::RPCFailed;
  }

  return rsp.error();
}
}  // namespace

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
                LOG_DEBUG(businessLogger, "[test_net handle called]");
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
}  // namespace wim
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
  businessLogger->info("service-post_verifycode] email as {}", email);

  rspInfo["error"] = requestVerifyCode(email);
  rspInfo["email"] = requestData["email"];
  return true;
}

bool Service::signUp(HttpSession::ResponsePtr response,
                     Json::Value &requestData) {
  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto username = requestData["username"].asString();
  auto password = requestData["password"].asString();
  auto email = requestData["email"].asString();
  businessLogger->info("[signUp]: start, username: {}, email: {}", username,
                       email);

  std::string verifycode = "1234";
  // verifycode logic...

  int hasEmail = db::MysqlDao::GetInstance()->hasEmail(email);
  if (hasEmail != 0) {
    businessLogger->info("[signUp]: email is exist");
    rspInfo["error"] = -1;
    return false;
  }

  int hasUsername = db::MysqlDao::GetInstance()->hasUsername(username);
  if (hasUsername != 0) {
    businessLogger->info("[signUp]: username is exist");
    rspInfo["error"] = -1;
    return false;
  }
  auto uid = db::RedisDao::GetInstance()->generateUserId();

  db::User::Ptr user(new db::User(0, uid, username, password, email));
  int hasUser = db::MysqlDao::GetInstance()->userRegister(user);
  if (hasUser != 0) {
    businessLogger->info("[signUp]: user or email exist");
    rspInfo["error"] = -1;
    return false;
  }

  // verifycode logic...
  businessLogger->info(
      "[signUp]: success! uid as {}, username as {}, email as {}", uid,
      username, email);
  rspInfo["error"] = 0;
  rspInfo["uid"] = Json::Value::Int64(uid);
  rspInfo["username"] = username;
  rspInfo["email"] = email;

  return true;
}

bool Service::signIn(HttpSession::ResponsePtr response,
                     Json::Value &requestData) {
  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  auto username = requestData["username"].asString();
  auto password = requestData["password"].asString();
  businessLogger->info("[signIn]: start, username: {}", username);

  auto user = wim::db::MysqlDao::GetInstance()->getUser(username);
  if (user == nullptr) {
    businessLogger->debug("user is not existd!");
    rspInfo["error"] = -1;
    return false;
  }

  if (user->password != password) {
    businessLogger->debug("password or email input is wrong!");
    rspInfo["error"] = -1;
    return false;
  }

  auto node = rpc::StateClient::GetInstance()->GetImServer(user->uid);
  if (node.empty()) {
    businessLogger->info("rpc request state service is fialed, user id as {} ",
                         user->uid);
    rspInfo["error"] = -1;
    return false;
  }

  auto hasFirstSignIn = db::MysqlDao::GetInstance()->hasUserInfo(user->uid);
  if (hasFirstSignIn == false)
    rspInfo["init"] = 1;
  else
    rspInfo["init"] = 0;
  businessLogger->info("[sigIn] user: {} login success", user->username);
  rspInfo["error"] = 0;
  rspInfo["uid"] = Json::Value::Int64(user->uid);
  rspInfo["ip"] = node.ip;
  rspInfo["port"] = node.port;

  // token 按 uid 覆盖并设置 TTL；持久化失败时不得返回可用登录结果。
  std::string chatAuthToken = GenerateChatAuthToken();
  if (!db::RedisDao::GetInstance()->setChatAuthToken(
          user->uid, chatAuthToken, kChatAuthTokenTtlSeconds)) {
    businessLogger->warn("[signIn] failed to persist chat auth token, uid: {}",
                         user->uid);
    rspInfo.clear();
    rspInfo["error"] = ErrorCodes::InternalError;
    return false;
  }
  rspInfo["chatToken"] = chatAuthToken;
  rspInfo["chatTokenExpiresIn"] = Json::Value::Int64(kChatAuthTokenTtlSeconds);

  return true;
}

bool Service::initUserinfo(HttpSession::ResponsePtr response,
                           Json::Value &requestData) {
  Json::Value rspInfo;
  Defer _([&]() { responseWrite(response, rspInfo.toStyledString()); });

  try {
    auto uid = requestData["uid"].asInt64();
    auto name = requestData["name"].asString();
    auto age = requestData["age"].asInt();
    auto sex = requestData["sex"].asString();
    auto headImageURI = requestData.asString();

    db::UserInfo::Ptr userInfo(
        new db::UserInfo(uid, name, age, sex, headImageURI));
    int hasUserInfo = db::MysqlDao::GetInstance()->insertUserInfo(userInfo);
    if (hasUserInfo != 0) {
      rspInfo["error"] = -1;
      businessLogger->debug("register user info failed");
      return false;
    }
    rspInfo["error"] = 0;
    return true;
  } catch (std::exception &e) {
    rspInfo["error"] = ErrorCodes::JsonParser;
    return false;
  }
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
  if (user == nullptr) {
    businessLogger->info(" user is not existd!");
    rspInfo["error"] = -1;
    return false;
  }
  bool updateSuccess = db::MysqlDao::GetInstance()->userModifyPassword(user);
  if (!updateSuccess) {
    businessLogger->error(" update password is failed");
    rspInfo["error"] = ErrorCodes::PasswdUpFailed;
    return false;
  }

  businessLogger->info("password updated for username: {}", username);

  rspInfo["error"] = 0;
  rspInfo["username"] = username;
  rspInfo["password"] = user->password;

  return true;
}
};  // namespace wim
