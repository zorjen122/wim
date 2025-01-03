#pragma once
#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"

class ServicePool : public Singleton<ServicePool>
{
	friend Singleton<ServicePool>;

public:
	using Service = boost::asio::io_context;
	using Work = boost::asio::io_context::work;
	using WorkPtr = std::unique_ptr<Work>;
	~ServicePool();
	ServicePool(const ServicePool &) = delete;
	ServicePool &operator=(const ServicePool &) = delete;
	boost::asio::io_context &GetService();
	void Stop();

private:
	ServicePool(std::size_t size = std::thread::hardware_concurrency());
	std::vector<Service> _ioServices;
	std::vector<WorkPtr> _works;
	std::vector<std::thread> _threads;
	std::size_t _nextIoService;
};
