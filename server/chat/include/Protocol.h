#pragma once
#include <string>
#include "Const.h"
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
class ServiceSystem;
class ChatSession;

namespace protocol
{
	class LogicPackage;

	class Package
	{
	public:
		Package(short size) : total(size), cur(0)
		{
			data = new char[total + 1]();
			data[total] = '\0';
		}

		~Package()
		{
			std::cout << "destruct Package\n";
			delete[] data;
		}

		void Clear()
		{
			::memset(data, 0, total);
			cur = 0;
		}

		short cur;
		short total;
		char *data;
	};

	class RecvPackage : public Package
	{
		friend class ServiceSystem;
		friend class LogicPackage;

	public:
		RecvPackage(short packageLen, short msgID);

		short id;
	};

	class SendPackage : public Package
	{
		friend class ServiceSystem;

	public:
		SendPackage(const char *msg, short max_len, short msg_id);

		short id;
	};

	class LogicPackage
	{
		friend class ServiceSystem;

	public:
		LogicPackage(std::shared_ptr<ChatSession>, std::shared_ptr<RecvPackage>);

		std::shared_ptr<ChatSession> _session;
		std::shared_ptr<RecvPackage> _recvPackage;
	};

};