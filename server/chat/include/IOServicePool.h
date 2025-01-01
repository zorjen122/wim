#pragma once
#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"

class IOServicePool : public Singleton<IOServicePool>
{
	friend Singleton<IOServicePool>;

public:
	using IOService = boost::asio::io_context;
	using Work = boost::asio::io_context::work;
	using WorkPtr = std::unique_ptr<Work>;
	~IOServicePool();
	IOServicePool(const IOServicePool &) = delete;
	IOServicePool &operator=(const IOServicePool &) = delete;
	boost::asio::io_context &GetIOService();
	void Stop();

private:
	IOServicePool(std::size_t size = std::thread::hardware_concurrency());
	std::vector<IOService> _ioServices;
	std::vector<WorkPtr> _works;
	std::vector<std::thread> _threads;
	std::size_t _nextIOService;
};
