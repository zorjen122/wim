#pragma once
#include <assert.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <mysql_connection.h>
#include <mysql_driver.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

#include "Singleton.h"

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

enum ErrorCodes {
  Success = 0,
  ErrorJsonParser = 1001,    // Json解析错误
  RPCFailed = 1002,          // RPC请求错误
  VerifyCodeExpired = 1003,  // 验证码过期
  VerifyCodeInvalid = 1004,  // 验证码错误
  DuplicateRegister = 1005,  // 用户已经存在
  PasswdErr = 1006,          // 密码错误
  EmailInvalid = 1007,       // 邮箱不匹配
  PasswdUpFailed = 1008,     // 更新密码失败
  PasswdInvalid = 1009,      // 密码更新失败
  TokenInvalid = 1010,       // Token失效
  UidInvalid = 1011,         // uid无效
};

// Defer类
class Defer {
 public:
  // 接受一个lambda表达式或者函数指针
  Defer(std::function<void()> func) : func_(func) {}

  // 析构函数中执行传入的函数
  ~Defer() { func_(); }

 private:
  std::function<void()> func_;
};

#define CODEPREFIX "code_"

// for ./build local path of the case
#define CONFIG_FILENAME "../config.ini"