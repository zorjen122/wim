#pragma once
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

using boost::asio::steady_timer;
using boost::asio::io_context;
using boost::system::error_code;

class Timer{

public:

  Timer(io_context& ioc, size_t seconds);
  ~Timer();
  void startTimer();
void resetTimer();
  virtual bool resetHandle();
  virtual void tickleHandle();

private:
    io_context& ioc;
    steady_timer timer;
};