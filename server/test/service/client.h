#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <json/json.h>
#include <string>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
namespace IM {

struct User {
  using ptr = std::shared_ptr<User>;
  User(long uid, const std::string &username, const std::string &password,
       const std::string &email)
      : uid(uid), username(username), password(password), email(email) {}
  long uid;
  std::string username;
  std::string password;
  std::string email;
};

struct Gate {
  struct ChatEndpoint {
    ChatEndpoint(const std::string &ip, const std::string &port)
        : ip(ip), port(port) {}
    std::string ip;
    std::string port;
  };

  Gate(net::io_context &iocontext, const std::string &ip,
       const std::string &port);

  ChatEndpoint signIn(const std::string &email, const std::string &password);
  bool signUp(const std::string &username, const std::string &password,
              const std::string &email);
  bool signOut();
  bool fogetPassword(const std::string &username);

  std::string __parseResponse();
  Json::Value __parseJson(const std::string &source);
  void __clearStatusMessage();

  net::io_context &context;
  beast::tcp_stream stream;
  beast::flat_buffer buffer;
  http::request<http::string_body> request;
  http::response<http::dynamic_body> response;
  std::map<int, User::ptr> users;
  tcp::resolver::results_type endpoint;

  bool onConnected;
};

struct Chat {

  Chat(net::io_context &iocontext, const std::string &ip,
       const std::string &port);

  net::io_context context;
  tcp::socket chat;
  std::string buffer;
};

struct Client {

  long uid;
  std::string username;
  std::string password;

  std::shared_ptr<Gate> gate;
  std::shared_ptr<Chat> chat;
};
}; // namespace IM