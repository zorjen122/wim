#include "IocPool.h"
#include "Logger.h"
#include "spdlog/spdlog.h"

namespace wim {
IocPool::IocPool(std::size_t size) : iocGroup(size), worker(size) {
  for (std::size_t i = 0; i < size; ++i)
    worker[i] = std::unique_ptr<Work>(new Work(iocGroup[i]));

  for (std::size_t i = 0; i < iocGroup.size(); ++i)
    threadGroup.emplace_back([this, i]() { iocGroup[i].run(); });

  if (iocGroup.size() >= 1)
    LOG_INFO(wim::netLogger, "IocPool created with {} threads",
             iocGroup.size());
  else
    LOG_WARN(wim::netLogger, "IocPool created is failed, size {} ",
             iocGroup.size());
}

IocPool::~IocPool() {
  spdlog::info("ServicePool::~ServicePool");
  Stop();
}

boost::asio::io_context &IocPool::GetContext() {
  static size_t currentIoc = 0;
  auto &ioc = iocGroup[currentIoc++];
  currentIoc = currentIoc % iocGroup.size();

  return ioc;
}

void IocPool::Stop() {
  // work 析构不会连带关闭控制的iocontext
  for (auto &work : worker) {
    work->get_io_context().stop();
    work.reset();
  }

  for (auto &t : threadGroup)
    t.join();
}
}; // namespace wim