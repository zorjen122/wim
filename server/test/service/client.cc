#include "client.h"
#include "base.h"
#include "global.h"
#include "json/value.h"
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/system/detail/error_code.hpp>

namespace IM {
Gate::Gate(net::io_context &iocontext, const std::string &host,
           const std::string &port)
    : context(iocontext), stream(iocontext) {
  if (host.empty() || port.empty())
    throw std::invalid_argument("host or port is empty");

  Defer _([this]() { __clearStatusMessage(); });

  tcp::resolver resolver(context);

  endpoint = resolver.resolve(host, port);

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::get);
  request.target(__GateTestPath__);
  request.version(11);
  request.set(http::field::host, host);
  request.set(http::field::content_type, "application/json");

  http::write(stream, request, ec);
  if (ec.failed())
    throw std::runtime_error("write failed: " + ec.message());

  http::read(stream, buffer, response, ec);
  if (ec.failed())
    throw std::runtime_error("read failed: " + ec.message());

  auto bodyBuffer = response.body().data();
  auto stringBody = beast::buffers_to_string(bodyBuffer);

  spdlog::info("response message: {}", stringBody);
}

Gate::ChatEndpoint Gate::signIn(const std::string &email,
                                const std::string &password) {
  spdlog::info("sign in as {}, password as {}", email, password);

  Defer _([this]() { __clearStatusMessage(); });

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::post);
  request.target(__GateSigninPath__);

  Json::Value requestData;
  requestData["email"] = email;
  requestData["password"] = password;

  request.body() = requestData.toStyledString();
  request.prepare_payload();
  http::write(stream, request, ec);
  if (ec.failed())
    throw std::runtime_error("write failed: " + ec.message());
  http::read(stream, buffer, response, ec);
  if (ec.failed())
    throw std::runtime_error("read failed: " + ec.message());

  auto stringBody = __parseResponse();
  spdlog::info("response status: {}", stringBody);

  Json::Reader reader;
  Json::Value responseData;
  reader.parse(stringBody, responseData);

  ChatEndpoint chatEndpoint(responseData["ip"].asString(),
                            responseData["port"].asString());

  return chatEndpoint;
}

bool Gate::signUp(const std::string &username, const std::string &password,
                  const std::string &email) {
  spdlog::info("sign in as {}, password as {}", username, password);

  Defer _([this]() { __clearStatusMessage(); });

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::post);
  request.target(__GateSignupPath__);

  Json::Value requestData;
  requestData["username"] = username;
  requestData["password"] = password;
  requestData["email"] = email;

  request.body() = requestData.toStyledString();
  request.prepare_payload();

  spdlog::info("http-write({}): request body: {}", request.target(),
               (request.body().data()));
  http::write(stream, request, ec);
  if (ec.failed())
    throw std::runtime_error("write failed: " + ec.message());
  http::read(stream, buffer, response, ec);
  if (ec.failed())
    throw std::runtime_error("read failed: " + ec.message());

  auto stringBody = __parseResponse();
  spdlog::info("response message: {} | status:{}", stringBody,
               response.result_int());

  Json::Value responseData = __parseJson(stringBody);
  auto uid = responseData["uid"].asInt();
  users[uid] = std::make_shared<User>(uid, username, password, email);

  return true;
}

bool Gate::signOut() {}
bool Gate::fogetPassword(const std::string &username) {}

std::string Gate::__parseResponse() {
  auto bodyBuffer = response.body().data();
  return beast::buffers_to_string(bodyBuffer);
}
Json::Value Gate::__parseJson(const std::string &source) {

  Json::Reader reader;
  Json::Value responseData;
  bool parseSuccess = reader.parse(source, responseData);
  if (parseSuccess == false)
    throw std::runtime_error("parse json failed");
  return responseData;
}

void Gate::__clearStatusMessage() {
  request.body().clear();
  response.body().clear();
  buffer.clear();
}
}; // namespace IM