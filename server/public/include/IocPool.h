#pragma once

#include "Const.h"

#include <boost/asio.hpp>
#include <vector>

namespace wim {
class IocPool : public Singleton<IocPool> {
  friend Singleton<IocPool>;

 public:
  using Service = boost::asio::io_context;
  using Work = boost::asio::executor_work_guard<Service::executor_type>;
  using WorkPtr = std::unique_ptr<Work>;
  ~IocPool();
  IocPool(const IocPool &) = delete;
  IocPool &operator=(const IocPool &) = delete;
  boost::asio::io_context &GetContext();
  void Stop();

 private:
  IocPool(std::size_t size = 2);
  std::vector<Service> iocGroup;
  std::vector<WorkPtr> worker;
  std::vector<std::thread> threadGroup;
};
};  // namespace wim
