#include "HttpSession.h"

#include "Service.h"
#include <boost/beast/http/verb.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>

namespace util {
unsigned char ToHex(unsigned char x) { return x > 9 ? x + 55 : x + 48; }

unsigned char FromHex(unsigned char x) {
  unsigned char y;
  if (x >= 'A' && x <= 'Z')
    y = x - 'A' + 10;
  else if (x >= 'a' && x <= 'z')
    y = x - 'a' + 10;
  else if (x >= '0' && x <= '9')
    y = x - '0';
  else
    assert(0);
  return y;
}

std::string UrlEncode(const std::string &str) {
  std::string strTemp = "";
  size_t length = str.length();
  for (size_t i = 0; i < length; i++) {

    if (isalnum((unsigned char)str[i]) || (str[i] == '-') || (str[i] == '_') ||
        (str[i] == '.') || (str[i] == '~'))
      strTemp += str[i];
    else if (str[i] == ' ')
      strTemp += "+";
    else {
      strTemp += '%';
      strTemp += util::ToHex((unsigned char)str[i] >> 4);
      strTemp += util::ToHex((unsigned char)str[i] & 0x0F);
    }
  }
  return strTemp;
}

std::string UrlDecode(const std::string &str) {
  std::string strTemp = "";
  size_t length = str.length();
  for (size_t i = 0; i < length; i++) {
    if (str[i] == '+')
      strTemp += ' ';
    else if (str[i] == '%') {
      assert(i + 2 < length);
      unsigned char high = util::FromHex((unsigned char)str[++i]);
      unsigned char low = util::FromHex((unsigned char)str[++i]);
      strTemp += high * 16 + low;
    } else
      strTemp += str[i];
  }
  return strTemp;
}

}; // namespace util

HttpSession::HttpSession(boost::asio::io_context &ioc)
    : socket(ioc), request(new http::request<http::dynamic_body>()),
      response(new http::response<http::dynamic_body>()) {}

void HttpSession::Start() {
  auto self = shared_from_this();
  http::async_read(socket, _buffer, *request,
                   [=](beast::error_code ec, std::size_t bytes_transferred) {
                     try {
                       if (ec) {
                         spdlog::info(
                             "http read err is {}, bytes_transferred is {}",
                             ec.message(), bytes_transferred);
                         return;
                       }

                       self->HandleRequest();
                       self->CheckDeadline();
                     } catch (std::exception &exp) {
                       std::cout << "exception is " << exp.what() << "\n";
                     }
                   });
}

void HttpSession::PreParseGetParam() {
  auto uri = request->target();
  auto query_pos = uri.find('?');
  if (query_pos == std::string::npos) {
    url = uri;
    return;
  }

  url = uri.substr(0, query_pos);
  std::string query_string = uri.substr(query_pos + 1);
  std::string key;
  std::string value;
  size_t pos = 0;
  while ((pos = query_string.find('&')) != std::string::npos) {
    auto pair = query_string.substr(0, pos);
    size_t eq_pos = pair.find('=');
    if (eq_pos != std::string::npos) {
      key = util::UrlDecode(pair.substr(0, eq_pos));
      value = util::UrlDecode(pair.substr(eq_pos + 1));
      paramMap[key] = value;
    }
    query_string.erase(0, pos + 1);
  }
  if (!query_string.empty()) {
    size_t eq_pos = query_string.find('=');
    if (eq_pos != std::string::npos) {
      key = util::UrlDecode(query_string.substr(0, eq_pos));
      value = util::UrlDecode(query_string.substr(eq_pos + 1));
      paramMap[key] = value;
    }
  }
}

void HttpSession::HandleRequest() {
  response->version(request->version());
  response->keep_alive(false);
  std::string path{};
  http::verb method = request->method();
  if (method == http::verb::get) {
    PreParseGetParam();
    path = url;
  } else if (method == http::verb::post) {
    path = request->target();
  }

  bool success =
      Service::GetInstance()->Handle(shared_from_this(), path, method);

  if (success) {
    response->result(http::status::ok);
    response->set(http::field::server, "GateServer");
  } else {
    response->result(http::status::not_found);
    response->set(http::field::content_type, "text/plain");
    beast::ostream(response->body()) << "url not found\r\n";
  }

  WriteResponse();
  return;
}

void HttpSession::CheckDeadline() {
  auto self = shared_from_this();

  deadline.async_wait([self](beast::error_code ec) {
    if (!ec) {
      self->socket.shutdown(tcp::socket::shutdown_both, ec);
    }
  });
}

void HttpSession::WriteResponse() {
  auto self = shared_from_this();

  response->content_length(response->body().size());

  http::async_write(socket, *response,
                    [self](beast::error_code ec, std::size_t) {
                      self->socket.shutdown(tcp::socket::shutdown_send, ec);
                      self->deadline.cancel();
                    });
}
