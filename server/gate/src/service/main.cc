#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "Const.h"
#include "GateServer.h"
#include "Configer.h"
#include "hiredis.h"
#include "RedisManager.h"
#include "MysqlManager.h"
#include "IOServicePool.h"

int main()
{
	try
	{
		MysqlManager::GetInstance();
		RedisManager::GetInstance();
		auto &gCfgMgr = Configer::GetInstance();
		std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
		unsigned short gate_port = atoi(gate_port_str.c_str());
		net::io_context ioc{1};
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc](const boost::system::error_code &error, int signal_number)
						   {
			if (error) 
				return;
			
			ioc.stop(); });
		std::make_shared<GateServer>(ioc, gate_port)->Start();
		std::cout << "Gate Server listen on port: " << gate_port << "\n";

		ioc.run();
		RedisManager::GetInstance()->Close();
	}
	catch (std::exception const &e)
	{
		std::cerr << "Error: " << e.what() << "\n";
		RedisManager::GetInstance()->Close();
		return EXIT_FAILURE;
	}
}
