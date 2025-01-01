#include "RedisManager.h"
#include "Const.h"
#include "Configer.h"

RedisManager::RedisManager()
{
	auto &gCfgMgr = Configer::GetInstance();
	auto host = gCfgMgr["Redis"]["Host"];
	auto port = gCfgMgr["Redis"]["Port"];
	auto pwd = gCfgMgr["Redis"]["Passwd"];
	_con_pool.reset(new RedisConPool(5, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}

RedisManager::~RedisManager()
{
}

bool RedisManager::Get(const std::string &key, std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "GET %s", key.c_str());
	if (reply == NULL)
	{
		std::cout << "[ GET  " << key << " ] failed" << "\n";
		// freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_STRING)
	{
		std::cout << "[ GET  " << key << " ] failed" << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	value = reply->str;
	freeReplyObject(reply);

	std::cout << "Succeed to execute command [ GET " << key << "  ]" << "\n";
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::Set(const std::string &key, const std::string &value)
{
	// 执行redis命令行
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

	// 如果返回NULL则说明执行失败
	if (NULL == reply)
	{
		std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << "\n";
		// freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	// 如果执行失败则释放连接
	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	// 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	std::cout << "Execut command [ SET " << key << "  " << value << " ] success ! " << "\n";
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::LPush(const std::string &key, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0)
	{
		std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::LPop(const std::string &key, std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr)
	{
		std::cout << "Execut command [ LPOP " << key << " ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL)
	{
		std::cout << "Execut command [ LPOP " << key << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	value = reply->str;
	std::cout << "Execut command [ LPOP " << key << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::RPush(const std::string &key, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0)
	{
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}
bool RedisManager::RPop(const std::string &key, std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr)
	{
		std::cout << "Execut command [ RPOP " << key << " ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL)
	{
		std::cout << "Execut command [ RPOP " << key << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}
	value = reply->str;
	std::cout << "Execut command [ RPOP " << key << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::HSet(const std::string &key, const std::string &hkey, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr)
	{
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::HSet(const char *key, const char *hkey, const char *hvalue, size_t hvaluelen)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	const char *argv[4];
	size_t argvlen[4];
	argv[0] = "HSET";
	argvlen[0] = 4;
	argv[1] = key;
	argvlen[1] = strlen(key);
	argv[2] = hkey;
	argvlen[2] = strlen(hkey);
	argv[3] = hvalue;
	argvlen[3] = hvaluelen;

	auto reply = (redisReply *)redisCommandArgv(connect, 4, argv, argvlen);
	if (reply == nullptr)
	{
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

std::string RedisManager::HGet(const std::string &key, const std::string &hkey)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return "";
	}
	const char *argv[3];
	size_t argvlen[3];
	argv[0] = "HGET";
	argvlen[0] = 4;
	argv[1] = key.c_str();
	argvlen[1] = key.length();
	argv[2] = hkey.c_str();
	argvlen[2] = hkey.length();

	auto reply = (redisReply *)redisCommandArgv(connect, 3, argv, argvlen);
	if (reply == nullptr)
	{
		std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return "";
	}

	if (reply->type == REDIS_REPLY_NIL)
	{
		freeReplyObject(reply);
		std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << "\n";
	return value;
}

bool RedisManager::HDel(const std::string &key, const std::string &field)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}

	Defer defer([&connect, this]()
				{ _con_pool->returnConnection(connect); });

	redisReply *reply = (redisReply *)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
	if (reply == nullptr)
	{
		std::cerr << "HDEL command failed" << "\n";
		return false;
	}

	bool success = false;
	if (reply->type == REDIS_REPLY_INTEGER)
	{
		success = reply->integer > 0;
	}

	freeReplyObject(reply);
	return success;
}

bool RedisManager::Del(const std::string &key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	auto reply = (redisReply *)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr)
	{
		std::cout << "Execut command [ Del " << key << " ] failure ! " << "\n";
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER)
	{
		std::cout << "Execut command [ Del " << key << " ] failure ! " << "\n";
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	std::cout << "Execut command [ Del " << key << " ] success ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisManager::ExistsKey(const std::string &key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}

	auto reply = (redisReply *)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr)
	{
		std::cout << "Not Found [ Key " << key << " ]  ! " << "\n";
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0)
	{
		std::cout << "Not Found [ Key " << key << " ]  ! " << "\n";
		_con_pool->returnConnection(connect);
		freeReplyObject(reply);
		return false;
	}
	std::cout << " Found [ Key " << key << " ] exists ! " << "\n";
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}
