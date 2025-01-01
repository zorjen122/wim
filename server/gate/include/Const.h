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
  ErrorJsonParser = 1001,    // Json��������
  RPCFailed = 1002,          // RPC�������
  VerifyCodeExpired = 1003,  // ��֤�����
  VerifyCodeInvalid = 1004,  // ��֤�����
  DuplicateRegister = 1005,  // �û��Ѿ�����
  PasswdErr = 1006,          // �������
  EmailInvalid = 1007,       // ���䲻ƥ��
  PasswdUpFailed = 1008,     // ��������ʧ��
  PasswdInvalid = 1009,      // �������ʧ��
  TokenInvalid = 1010,       // TokenʧЧ
  UidInvalid = 1011,         // uid��Ч
};

// Defer��
class Defer {
 public:
  // ����һ��lambda���ʽ���ߺ���ָ��
  Defer(std::function<void()> func) : func_(func) {}

  // ����������ִ�д���ĺ���
  ~Defer() { func_(); }

 private:
  std::function<void()> func_;
};

#define CODEPREFIX "code_"

// for ./build local path of the case
#define CONFIG_FILENAME "../config.ini"