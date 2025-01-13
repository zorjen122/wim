#pragma once

#include "Const.h"

#include <boost/asio.hpp>
#include <vector>

class IoContextPool : public Singleton<IoContextPool> {
  friend Singleton<IoContextPool>;

public:
  using Service = boost::asio::io_context;
  using Work = boost::asio::io_context::work;
  using WorkPtr = std::unique_ptr<Work>;
  ~IoContextPool();
  IoContextPool(const IoContextPool &) = delete;
  IoContextPool &operator=(const IoContextPool &) = delete;
  boost::asio::io_context &GetContext();
  void Stop();

private:
  IoContextPool(std::size_t size = std::thread::hardware_concurrency());
  std::vector<Service> iocGroup;
  std::vector<WorkPtr> worker;
  std::vector<std::thread> threadGroup;
};
