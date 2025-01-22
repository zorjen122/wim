#include "Protocol.h"
#include "Const.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <climits>

namespace protocol {
RecvPackage::RecvPackage(unsigned int maxLen, unsigned int msgID)
    : Package(boost::asio::detail::socket_ops::network_to_host_long(maxLen) +
              PACKAGE_TOTAL_LEN),
      id(boost::asio::detail::socket_ops::network_to_host_long(msgID)) {

  memcpy(data, &id, PACKAGE_ID_LEN);
  memcpy(data + PACKAGE_ID_LEN, &total, PACKAGE_DATA_SIZE_LEN);
}

SendPackage::SendPackage(const char *msg, unsigned int maxLen,
                         unsigned int msgID)
    : Package(maxLen + PACKAGE_TOTAL_LEN), id(msgID) {

  unsigned int tmpID =
      boost::asio::detail::socket_ops::host_to_network_long(id);
  unsigned int tmpLen =
      boost::asio::detail::socket_ops::host_to_network_long(maxLen);
  if (tmpLen >= UINT_MAX) {
    spdlog::warn("[Send-Package] too big");
  }

  memcpy(data, &tmpID, PACKAGE_ID_LEN);
  memcpy(data + PACKAGE_ID_LEN, &tmpLen, PACKAGE_DATA_SIZE_LEN);
  memcpy(data + PACKAGE_ID_LEN + PACKAGE_DATA_SIZE_LEN, msg, maxLen);
}

LogicPackage::LogicPackage(std::shared_ptr<ChatSession> chatSession,
                           std::shared_ptr<RecvPackage> package)
    : session(chatSession), recvPackage(package) {}

}; // namespace protocol