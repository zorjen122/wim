#include "IOServicePool.h"
#include <iostream>

ServicePool::ServicePool(std::size_t size) : _ioServices(size), _works(size), _nextIoService(0)
{
	for (std::size_t i = 0; i < size; ++i)
		_works[i] = std::unique_ptr<Work>(new Work(_ioServices[i]));

	// 遍历多个ioservice，创建多个线程，每个线程内部启动ioservice
	for (std::size_t i = 0; i < _ioServices.size(); ++i)
		_threads.emplace_back([this, i]()
							  { _ioServices[i].run(); });
}

ServicePool::~ServicePool()
{
	std::cout << "IOServicePool destruct" << endl;
}

boost::asio::io_context &ServicePool::GetService()
{
	auto &service = _ioServices[_nextIoService++];
	if (_nextIoService == _ioServices.size())
		_nextIoService = 0;
	return service;
}

void ServicePool::Stop()
{
	// 因为仅仅执行work.reset并不能让iocontext从run的状态中退出
	// 当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
	for (auto &work : _works)
	{
		work->get_io_context().stop();
		work.reset();
	}

	for (auto &t : _threads)
		t.join();
}
