#pragma once
#include "net.h"
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <json/json.h>
#include <string>

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
struct UserInfo {
  using Ptr = std::shared_ptr<UserInfo>;

  UserInfo(size_t uid, std::string name, short age, std::string sex,
           std::string headImageURL)
      : uid(std::move(uid)), name(std::move(name)), age(std::move(age)),
        sex(std::move(sex)), headImageURL(std::move(headImageURL)) {}

  size_t uid;
  std::string name;
  short age;
  std::string sex;
  std::string headImageURL;
};
struct Endpoint {
  Endpoint(const std::string &ip, const std::string &port)
      : ip(ip), port(port) {}
  std::string ip;
  std::string port;
};
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

  bool initUserInfo(UserInfo::Ptr userInfo);

  std::string __parseResponse();
  void __clearStatusMessage();

  net::io_context &context;
  beast::tcp_stream stream;
  beast::flat_buffer buffer;
  http::request<http::string_body> request;
  http::response<http::dynamic_body> response;
  std::map<std::string, User::ptr> users;
  tcp::resolver::results_type endpoint;

  bool onConnected;
};

struct Chat {
  Chat(net::io_context &iocontext, Endpoint endpoint, User::ptr user,
       UserInfo::Ptr userInfo);

  bool login(bool isFirstLogin = true);
  long searchUser(const std::string &username);
  bool addFriend(long uid, const std::string &requestMessage);

  void sendMessage(long uid, const std::string &message);

  void run();

  User::ptr user;
  net::io_context &context;
  Endpoint endpoint;
  std::shared_ptr<tcp::socket> chat;
  UserInfo::Ptr userInfo;
  char buffer[4096] = {0};
};

struct Client {

  long uid;
  std::string username;
  std::string password;

  std::shared_ptr<Gate> gate;
  std::shared_ptr<Chat> chat;
};
}; // namespace IM
