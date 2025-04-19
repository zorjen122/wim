#include "client.h"
#include "Const.h"
#include "global.h"
#include "service/net.h"
#include "json/value.h"

#include <boost/asio/io_context.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cassert>
#include <spdlog/spdlog.h>
#include <string>

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

std::pair<Endpoint, int> Gate::signIn(const std::string &username,
                                      const std::string &password) {
  spdlog::info("sign in as {}, password as {}", username, password);

  Defer _([this]() { __clearStatusMessage(); });

  boost::system::error_code ec;
  stream.connect(endpoint, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());

  request.method(http::verb::post);
  request.target(__GateSigninPath__);

  Json::Value requestData;
  requestData["username"] = username;
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

  int init = responseData["init"].asInt();

  Endpoint chatEndpoint(responseData["ip"].asString(),
                        responseData["port"].asString());

  return {chatEndpoint, init};
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
  auto uid = responseData["uid"].asInt64();
  users[username] = std::make_shared<User>(uid, username, password, email);

  return true;
}
bool Gate::signOut() {}
bool Gate::fogetPassword(const std::string &username) {}

std::string Gate::__parseResponse() {
  auto bodyBuffer = response.body().data();
  return beast::buffers_to_string(bodyBuffer);
}
Json::Value __parseJson(const std::string &source) {
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

Chat::Chat(net::io_context &iocontext, Endpoint endpoint, User::ptr user,
           UserInfo::Ptr userInfo)
    : context(iocontext), endpoint(endpoint), user(user), userInfo(userInfo) {
  chat.reset(new tcp::socket(context));

  boost::system::error_code ec;

  spdlog::info("chat connect to {}:{}", endpoint.ip, endpoint.port);
  net::ip::tcp::endpoint ep(net::ip::address::from_string(endpoint.ip),
                            std::stoi(endpoint.port));

  chat->connect(ep, ec);
  if (ec.failed())
    throw std::runtime_error("connect failed: " + ec.message());
}

bool Chat::login(bool isFirstLogin) {

  Json::Value loginRequest;
  loginRequest["uid"] = Json::Value::Int64(user->uid);

  if (isFirstLogin) {
    loginRequest["init"] = true;
    loginRequest["uid"] = Json::Value::Int64(userInfo->uid);
    loginRequest["name"] = userInfo->name;
    loginRequest["age"] = userInfo->age;
    loginRequest["sex"] = userInfo->sex;
    loginRequest["headImageURL"] = userInfo->headImageURL;
  }

  base::pushMessage(chat, ID_LOGIN_INIT_REQ, loginRequest.toStyledString());
  auto source = base::recviceMessage(chat);

  Json::Value loginRsp = __parseJson(source);
  auto errcode = loginRsp["error"].asInt();
  if (errcode == -1) {
    return false;
  }

  std::string name = loginRsp["name"].asString();
  short age = loginRsp["age"].asInt();
  short sex = loginRsp["sex"].asInt();
  std::string headImageURL = loginRsp["headImageURL"].asString();

  spdlog::info("userInfo: uid {}, name {}, age {}, sex {}, headImageURL {}",
               name, age, sex, headImageURL);

  return true;
}

void Chat::run() { IM::readRun(chat, buffer); }
long Chat::searchUser(const std::string &username) {
  Json::Value searchReq;
  searchReq["username"] = username;
  base::pushMessage(chat, ID_SEARCH_USER_REQ, searchReq.toStyledString());
  auto source = base::recviceMessage(chat);
  Json::Value searchRsp = __parseJson(source);

  auto errcode = searchReq["error"].asInt();
  if (errcode == -1) {
    return false;
  }
  long uid = searchRsp["uid"].asInt64();
  std::string name = searchRsp["name"].asString();
  short age = searchRsp["age"].asInt();
  short sex = searchRsp["sex"].asInt();
  std::string headImageURL = searchRsp["headImageURL"].asString();

  spdlog::info("userInfo: uid {}, name {}, age {}, sex {}, headImageURL {}",
               name, age, sex, headImageURL);

  return uid;
}

bool Chat::addFriend(long uid, const std::string &requestMessage) {
  Json::Value addFriendReq;
  addFriendReq["fromUid"] = Json::Value::Int64(user->uid);
  addFriendReq["toUid"] = Json::Value::Int64(uid);
  addFriendReq["requestMessage"] = requestMessage;
  base::pushMessage(chat, ID_NOTIFY_ADD_FRIEND_REQ,
                    addFriendReq.toStyledString());
  auto source = base::recviceMessage(chat);
  Json::Value addFriendRsp = __parseJson(source);
  auto errcode = addFriendRsp["error"].asInt();
  if (errcode == -1) {
    spdlog::error("add friend failed");
    return false;
  }
  return true;
}

void Chat::sendMessage(long uid, const std::string &message) {
  Json::Value textMsg;
  static std::atomic<int> seq{0};

  // 该id仅是客户端的序列号，其作用是在之后接收到服务器ACK能找到并取消重传定时器
  textMsg["id"] = seq.load();
  seq += 1;
  textMsg["fromUid"] = Json::Value::Int64(user->uid);
  textMsg["toUid"] = Json::Value::Int64(uid);
  textMsg["text"] = message;
  IM::onReWrite(seq, chat, textMsg.toStyledString(), ID_TEXT_SEND_REQ);
}

}; // namespace IM