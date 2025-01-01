#include "ServiceSystem.h"

#include "Const.h"
#include "HttpConnection.h"
#include "MysqlManager.h"
#include "RedisManager.h"
#include "RpcStatusClient.h"
#include "VerifyGrpcClient.h"
#include "spdlog/spdlog.h"

ServiceSystem::ServiceSystem() {
  RegisterGet("/test_get", [](std::shared_ptr<HttpConnection> connection) {
    beast::ostream(connection->_response.body()) << "receive test_get req "
                                                 << "\n";
  });

  RegisterPost(
      "/post_verifycode", [](std::shared_ptr<HttpConnection> connection) {
        auto buffer = connection->_request.body().data();
        auto body = boost::beast::buffers_to_string(buffer);

        spdlog::info("receive body is {}", body);

        connection->_response.set(http::field::content_type, "text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        std::string rsp_package;

        Defer defer([&root, &connection, &rsp_package]() {
          rsp_package = root.toStyledString();
          beast::ostream(connection->_response.body()) << rsp_package;
        });

        bool parse_success = reader.parse(body, src_root);
        if (!parse_success) {
          spdlog::info("[post_verifycode] Failed to parse JSON data!");
          root["error"] = ErrorCodes::ErrorJsonParser;
          return false;
        }

        if (!src_root.isMember("email")) {
          spdlog::info("[post_verifycode-email] Failed to parse JSON data!");

          root["error"] = ErrorCodes::ErrorJsonParser;
          return false;
        }

        auto email = src_root["email"].asString();
        GetVerifyRsp rsp =
            VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
        spdlog::info("service-post_verifycode] email as {}", email);

        root["error"] = rsp.error();
        root["email"] = src_root["email"];
        return true;
      });

  /*
    package
    {
      email : value
      name : value
      pwd : value
      confirm : value
      icon : value
    }
  */
  RegisterPost(
      "/post_register", [](std::shared_ptr<HttpConnection> connection) {
        auto buffer = connection->_request.body().data();
        auto body = boost::beast::buffers_to_string(buffer);

        spdlog::info("[post_register] receive body is {}", body);

        connection->_response.set(http::field::content_type, "text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        std::string rsp_package;

        Defer defer([&root, &connection, &rsp_package]() {
          rsp_package = root.toStyledString();
          beast::ostream(connection->_response.body()) << rsp_package;
        });

        bool parse_success = reader.parse(body, src_root);
        if (!parse_success) {
          spdlog::info("[post_register] Failed to parse JSON data");
          root["error"] = ErrorCodes::ErrorJsonParser;
          return true;
        }

        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["password"].asString();
        auto confirm = src_root["confirm"].asString();
        auto icon = src_root["icon"].asString();

        if (pwd != confirm) {
          spdlog::info("[post_register] password is wrong");
          root["error"] = ErrorCodes::PasswdErr;
          return true;
        }

        std::string verifycode;
        // bool has_verifycode = RedisManager::GetInstance()->Get(
        //     CODEPREFIX + src_root["email"].asString(), verifycode);
        // if (!has_verifycode) {
        //   spdlog::info("[post_register] get Verify code expired");
        //   root["error"] = ErrorCodes::VerifyCodeExpired;
        //   return true;
        // }

        // if (verifycode != src_root["verifycode"].asString()) {
        //   spdlog::info("[post_register] Verify code error");
        //   root["error"] = ErrorCodes::VerifyCodeInvalid;
        //   return true;
        // }

        int uid = MysqlManager::GetInstance()->RegisterUser(name, email, pwd);
        if (uid == 0 || uid == -1) {
          spdlog::info("[post_register] user or email exist");
          root["error"] = ErrorCodes::DuplicateRegister;
          return true;
        }

        root["error"] = 0;
        root["uid"] = uid;
        root["email"] = email;
        root["user"] = name;
        root["password"] = pwd;
        root["confirm"] = confirm;
        root["icon"] = icon;
        root["verifycode"] = src_root["verifycode"].asString();

        rsp_package = root.toStyledString();
        beast::ostream(connection->_response.body()) << rsp_package;
        return true;
      });

  RegisterPost("/post_reset", [](std::shared_ptr<HttpConnection> connection) {
    auto buffer = connection->_request.body().data();
    auto body = boost::beast::buffers_to_string(buffer);
    spdlog::info("[post_reset] receive body is {}", body);
    connection->_response.set(http::field::content_type, "text/json");
    Json::Value root;
    Json::Reader reader;
    Json::Value src_root;
    std::string rsp_package;

    Defer defer([&root, &connection, &rsp_package]() {
      std::string rsp_package = root.toStyledString();
      beast::ostream(connection->_response.body()) << rsp_package;
    });

    bool parse_success = reader.parse(body, src_root);
    if (!parse_success) {
      spdlog::error("[post_reset] Failed to parse JSON data");
      root["error"] = ErrorCodes::ErrorJsonParser;
      return false;
    }

    auto email = src_root["email"].asString();
    auto name = src_root["user"].asString();
    auto pwd = src_root["password"].asString();

    std::string verifycode;
    bool has_verifycode = RedisManager::GetInstance()->Get(
        CODEPREFIX + src_root["email"].asString(), verifycode);
    if (!has_verifycode) {
      spdlog::info("[post-reset] Verify code is expired");
      root["error"] = ErrorCodes::VerifyCodeExpired;
      return false;
    }

    if (verifycode != src_root["verifycode"].asString()) {
      spdlog::info("[post-reset] verifycode is invalid");

      root["error"] = ErrorCodes::VerifyCodeInvalid;
      return false;
    }
    bool email_valid = MysqlManager::GetInstance()->CheckEmail(name, email);
    if (!email_valid) {
      spdlog::info("[post-reset] email input is invalid");
      root["error"] = ErrorCodes::EmailInvalid;
      return false;
    }

    bool update_success =
        MysqlManager::GetInstance()->UpdatePassword(name, pwd);
    if (!update_success) {
      spdlog::error(" update password is failed");
      root["error"] = ErrorCodes::PasswdUpFailed;
      return false;
    }

    spdlog::info("[post-reset] success! new password is {}", pwd);

    root["error"] = 0;
    root["email"] = email;
    root["user"] = name;
    root["password"] = pwd;
    root["verifycode"] = src_root["verifycode"].asString();

    rsp_package = root.toStyledString();
    beast::ostream(connection->_response.body()) << rsp_package;
    return true;
  });

  RegisterPost("/post_login", [](std::shared_ptr<HttpConnection> connection) {
    auto buffer = connection->_request.body().data();
    auto body = boost::beast::buffers_to_string(buffer);
    connection->_response.set(http::field::content_type, "text/json");

    spdlog::info("[post-login]: receive body is {}", body);

    Json::Value root;
    Json::Reader reader;
    Json::Value src_root;
    std::string rsp_package;

    Defer defer([&rsp_package, &connection, &root]() {
      rsp_package = root.toStyledString();
      beast::ostream(connection->_response.body()) << rsp_package;
    });

    bool parse_success = reader.parse(body, src_root);
    if (!parse_success) {
      spdlog::error("parser json to string is wrong!");
      root["error"] = ErrorCodes::ErrorJsonParser;
      return false;
    }

    UserInfo userInfo;
    auto email = src_root["email"].asString();
    auto pwd = src_root["password"].asString();
    spdlog::info("debug: email: {}, pwd: {} ", email, pwd);
    
    bool userIsExist =
        MysqlManager::GetInstance()->CheckUserExist(email, pwd, userInfo);
    if (!userIsExist) {
      spdlog::error("[post_login: password or email input is wrong!]");
      root["error"] = ErrorCodes::PasswdInvalid;
      return false;
    }

    auto reply = StatusGrpcClient::GetInstance()->GetImServer(userInfo.uid);
    if (reply.error()) {
      spdlog::error("[grpc about gateway im server is failed, error as {}",
                    reply.error());
      root["error"] = ErrorCodes::RPCFailed;
      return false;
    }

    spdlog::info("[post-login] success!, user id as {} ", userInfo.uid);
    root["error"] = 0;
    root["email"] = email;
    root["uid"] = userInfo.uid;
    root["token"] = reply.token();
    root["host"] = reply.host();
    root["port"] = reply.port();
    rsp_package = root.toStyledString();

    return true;
  });
}

void ServiceSystem::RegisterGet(std::string url, HttpHandler handler) {
  auto ret = _get_handlers.insert(make_pair(url, handler));
  if(ret.second == false)
  {
    spdlog::error("RegisterGet insert is wrong!");
  }
}

void ServiceSystem::RegisterPost(std::string url, HttpHandler handler) {
  auto ret = _post_handlers.insert(make_pair(url, handler));
  if(ret.second == false)
  {
    spdlog::error("RegisterPost insert is wrong!");
  }
}

ServiceSystem::~ServiceSystem() {}

bool ServiceSystem::HandleGet(std::string path,
                              std::shared_ptr<HttpConnection> con) {
  if (_get_handlers.find(path) == _get_handlers.end()) {
    return false;
  }

  _get_handlers[path](con);
  return true;
}

bool ServiceSystem::HandlePost(std::string path,
                               std::shared_ptr<HttpConnection> con) {
  if (_post_handlers.find(path) == _post_handlers.end()) {
    return false;
  }

  _post_handlers[path](con);
  return true;
}