#pragma once
#include <functional>

enum ErrorCodes
{
	Success = 0,
	Error_Json = 1001,	   // Json��������
	RPCFailed = 1002,	   // RPC�������
	VarifyExpired = 1003,  // ��֤�����
	VarifyCodeErr = 1004,  // ��֤�����
	UserExist = 1005,	   // �û��Ѿ�����
	PasswdErr = 1006,	   // �������
	EmailNotMatch = 1007,  // ���䲻ƥ��
	PasswdUpFailed = 1008, // ��������ʧ��
	PasswdInvalid = 1009,  // �������ʧ��
	TokenInvalid = 1010,   // TokenʧЧ
	UidInvalid = 1011,	   // uid��Ч
};

// Defer��
class Defer
{
public:
	// ����һ��lambda���ʽ���ߺ���ָ��
	Defer(std::function<void()> func) : func_(func) {}

	// ����������ִ�д���ĺ���
	~Defer()
	{
		func_();
	}

private:
	std::function<void()> func_;
};

#define MAX_LENGTH 1024 * 2
// ͷ���ܳ���
#define HEAD_TOTAL_LEN 4
// ͷ��id����
#define HEAD_ID_LEN 2
// ͷ�����ݳ���
#define HEAD_DATA_LEN 2
#define MAX_RECV_QUEUE_LEN 10000
#define MAX_SEND_QUEUE_LEN 1000

enum ServiceId
{
	IP_PING_PONG_REQ = 1003,			// ����������Ping����
	IP_PING_PONG_RSP = 1004,			// ����������Pong�ذ�
	ID_CHAT_LOGIN_INIT = 1005,			// �û���½
	ID_CHAT_LOGIN_INIT_RSP = 1006,		// �û���½�ذ�
	ID_SEARCH_USER_REQ = 1007,			// �û���������
	ID_SEARCH_USER_RSP = 1008,			// �����û��ذ�
	ID_ADD_FRIEND_REQ = 1009,			// ������Ӻ�������
	ID_ADD_FRIEND_RSP = 1010,			// ������Ӻ��ѻظ�
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,	// ֪ͨ�û���Ӻ�������
	ID_AUTH_FRIEND_REQ = 1013,			// ��֤��������
	ID_AUTH_FRIEND_RSP = 1014,			// ��֤���ѻظ�
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015,	// ֪ͨ�û���֤��������
	ID_TEXT_CHAT_MSG_REQ = 1017,		// �ı�������Ϣ����
	ID_TEXT_CHAT_MSG_RSP = 1018,		// �ı�������Ϣ�ظ�
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, // ֪ͨ�û��ı�������Ϣ

};

// Redis�ֶ�ǰ׺

#define PREFIX_REDIS_UIP "uip_"
#define PREFIX_REDIS_USER_TOKEN "utoken_"
#define PREFIX_REDIS_IP_COUNT "ipcount_"
#define PREFIX_REDIS_USER_INFO "ubaseinfo_"
#define PREFIX_REDIS_USER_ACTIVE_COUNT "logincount"
#define PREFIX_REDIS_NAME_INFO "nameinfo_"