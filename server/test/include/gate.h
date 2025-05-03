#pragma once

#include "DbGlobal.h"
#include "chatSession.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <jsoncpp/json/json.h>
#include <string>

namespace wim {
Json::Value __parseJson(const std::string &source);

struct Gate {

  Gate(net::io_context &iocontext, const std::string &ip,
       const std::string &port);

  std::pair<Endpoint, int> signIn(const std::string &username,
                                  const std::string &password);
  bool signUp(const std::string &username, const std::string &password,
              const std::string &email);
  bool signOut();
  bool fogetPassword(const std::string &username);

  bool initUserInfo(db::UserInfo::Ptr userInfo);

  std::string __parseResponse();
  void __clearStatusMessage();

  net::io_context &context;
  beast::tcp_stream stream;
  beast::flat_buffer buffer;
  http::request<http::string_body> request;
  http::response<http::dynamic_body> response;
  std::map<std::string, db::User::Ptr> users;
  tcp::resolver::results_type endpoint;

  bool onConnected;
};
} // namespace wim