#pragma once
#include <functional>

enum ErrorCodes
{
	Success = 0,
	Error_Json = 1001,	   // Json解析错误
	RPCFailed = 1002,	   // RPC请求错误
	VarifyExpired = 1003,  // 验证码过期
	VarifyCodeErr = 1004,  // 验证码错误
	UserExist = 1005,	   // 用户已经存在
	PasswdErr = 1006,	   // 密码错误
	EmailNotMatch = 1007,  // 邮箱不匹配
	PasswdUpFailed = 1008, // 更新密码失败
	PasswdInvalid = 1009,  // 密码更新失败
	TokenInvalid = 1010,   // Token失效
	UidInvalid = 1011,	   // uid无效
};

// Defer类
class Defer
{
public:
	// 接受一个lambda表达式或者函数指针
	Defer(std::function<void()> func) : func_(func) {}

	// 析构函数中执行传入的函数
	~Defer()
	{
		func_();
	}

private:
	std::function<void()> func_;
};

#define MAX_LENGTH 1024 * 2
// 头部总长度
#define HEAD_TOTAL_LEN 4
// 头部id长度
#define HEAD_ID_LEN 2
// 头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECV_QUEUE_LEN 10000
#define MAX_SEND_QUEUE_LEN 1000

enum ServiceId
{
	IP_PING_PONG_REQ = 1003,			// 长链接心跳Ping请求
	IP_PING_PONG_RSP = 1004,			// 长链接心跳Pong回包
	ID_CHAT_LOGIN_INIT = 1005,			// 用户登陆
	ID_CHAT_LOGIN_INIT_RSP = 1006,		// 用户登陆回包
	ID_SEARCH_USER_REQ = 1007,			// 用户搜索请求
	ID_SEARCH_USER_RSP = 1008,			// 搜索用户回包
	ID_ADD_FRIEND_REQ = 1009,			// 申请添加好友请求
	ID_ADD_FRIEND_RSP = 1010,			// 申请添加好友回复
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,	// 通知用户添加好友申请
	ID_AUTH_FRIEND_REQ = 1013,			// 认证好友请求
	ID_AUTH_FRIEND_RSP = 1014,			// 认证好友回复
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015,	// 通知用户认证好友申请
	ID_TEXT_CHAT_MSG_REQ = 1017,		// 文本聊天信息请求
	ID_TEXT_CHAT_MSG_RSP = 1018,		// 文本聊天信息回复
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, // 通知用户文本聊天信息

};

// Redis字段前缀

#define PREFIX_REDIS_UIP "uip_"
#define PREFIX_REDIS_USER_TOKEN "utoken_"
#define PREFIX_REDIS_IP_COUNT "ipcount_"
#define PREFIX_REDIS_USER_INFO "ubaseinfo_"
#define PREFIX_REDIS_USER_ACTIVE_COUNT "logincount"
#define PREFIX_REDIS_NAME_INFO "nameinfo_"