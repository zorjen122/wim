#pragma once
#include <functional>
#include <map>

#include "Const.h"
#include "Singleton.h"

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;
class ServiceSystem : public Singleton<ServiceSystem> {
  friend class Singleton<ServiceSystem>;

 public:
  ~ServiceSystem();
  bool HandleGet(std::string, std::shared_ptr<HttpConnection>);
  void RegisterGet(std::string, HttpHandler handler);
  void RegisterPost(std::string, HttpHandler handler);
  bool HandlePost(std::string, std::shared_ptr<HttpConnection>);

 private:
  ServiceSystem();
  std::map<std::string, HttpHandler> _post_handlers;
  std::map<std::string, HttpHandler> _get_handlers;
};
