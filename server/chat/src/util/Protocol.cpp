#include "Protocol.h"

namespace protocol
{
	RecvPackage::RecvPackage(short max_len, short msg_id) : Package(max_len),
															id(msg_id)
	{
	}

	SendPackage::SendPackage(const char *msg, short max_len, short msg_id) : Package(max_len + PACKAGE_TOTAL_LEN), id(msg_id)
	{
		short msgID = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
		memcpy(data, &msgID, PACKAGE_ID_LEN);
		short msgTotal = boost::asio::detail::socket_ops::host_to_network_short(max_len);
		memcpy(data + PACKAGE_ID_LEN, &msgTotal, PACKAGE_DATA_LEN);
		memcpy(data + PACKAGE_ID_LEN + PACKAGE_DATA_LEN, msg, max_len);
	}

	LogicPackage::LogicPackage(std::shared_ptr<ChatSession> session,
							   std::shared_ptr<RecvPackage> recvPackage) : _session(session), _recvPackage(recvPackage)
	{
	}

};