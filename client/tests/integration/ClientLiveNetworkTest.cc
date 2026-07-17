#include "adapters/connection_gateway/ClientProtocol.h"
#include "adapters/connection_gateway/ConnectionGatewayClient.h"
#include "adapters/gate/GateHttpClient.h"
#include "tcp_message.pb.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

namespace wim::client {
namespace {

bool ParsePacket(const QByteArray &payload, wim::protocol::Packet *packet) {
  return packet != nullptr &&
         packet->ParseFromArray(payload.constData(), payload.size());
}

QString RequiredEnvironment(const char *name) {
  return QString::fromUtf8(qgetenv(name));
}

}  // namespace

class ClientLiveNetworkTest final : public QObject {
  Q_OBJECT

 private slots:
  void authSendAndSyncAcrossRealGateways();
};

void ClientLiveNetworkTest::authSendAndSyncAcrossRealGateways() {
  const QString gateUrl = RequiredEnvironment("WIM_CLIENT_LIVE_GATE_URL");
  const QString userA = RequiredEnvironment("WIM_CLIENT_LIVE_USER_A");
  const QString passwordA = RequiredEnvironment("WIM_CLIENT_LIVE_PASSWORD_A");
  const QString userB = RequiredEnvironment("WIM_CLIENT_LIVE_USER_B");
  const QString passwordB = RequiredEnvironment("WIM_CLIENT_LIVE_PASSWORD_B");
  if (gateUrl.isEmpty() || userA.isEmpty() || passwordA.isEmpty() ||
      userB.isEmpty() || passwordB.isEmpty()) {
    QSKIP("Set WIM_CLIENT_LIVE_GATE_URL and both live user/password pairs");
  }

  GateHttpClient gateA;
  GateHttpClient gateB;
  gateA.SetBaseUrl(QUrl(gateUrl));
  gateB.SetBaseUrl(QUrl(gateUrl));
  QSignalSpy gateSuccessA(&gateA, &GateHttpClient::SignInSucceeded);
  QSignalSpy gateSuccessB(&gateB, &GateHttpClient::SignInSucceeded);
  QSignalSpy gateFailureA(&gateA, &GateHttpClient::OperationFailed);
  QSignalSpy gateFailureB(&gateB, &GateHttpClient::OperationFailed);

  gateA.SignIn(userA, passwordA);
  gateB.SignIn(userB, passwordB);
  QTRY_VERIFY_WITH_TIMEOUT(
      gateSuccessA.count() == 1 || gateFailureA.count() == 1, 10'000);
  QTRY_VERIFY_WITH_TIMEOUT(
      gateSuccessB.count() == 1 || gateFailureB.count() == 1, 10'000);
  if (!gateFailureA.isEmpty()) {
    QFAIL(qPrintable(gateFailureA.constFirst().at(2).toString()));
  }
  if (!gateFailureB.isEmpty()) {
    QFAIL(qPrintable(gateFailureB.constFirst().at(2).toString()));
  }

  const GateSession sessionA =
      qvariant_cast<GateSession>(gateSuccessA.constFirst().at(0));
  const GateSession sessionB =
      qvariant_cast<GateSession>(gateSuccessB.constFirst().at(0));
  QVERIFY(sessionA.uid > 0);
  QVERIFY(sessionB.uid > 0);
  QVERIFY(sessionA.uid != sessionB.uid);
  QVERIFY(!sessionA.gatewayId.isEmpty());
  QVERIFY(!sessionB.gatewayId.isEmpty());

  ConnectionGatewayClient gatewayA;
  ConnectionGatewayClient gatewayB;
  QSignalSpy authenticatedA(&gatewayA, &ConnectionGatewayClient::Authenticated);
  QSignalSpy authenticatedB(&gatewayB, &ConnectionGatewayClient::Authenticated);
  QSignalSpy responsesA(&gatewayA, &ConnectionGatewayClient::ResponseReceived);
  QSignalSpy responsesB(&gatewayB, &ConnectionGatewayClient::ResponseReceived);
  QSignalSpy pushesB(&gatewayB, &ConnectionGatewayClient::PushReceived);
  QSignalSpy failuresA(&gatewayA, &ConnectionGatewayClient::RequestFailed);
  QSignalSpy failuresB(&gatewayB, &ConnectionGatewayClient::RequestFailed);
  QSignalSpy protocolErrorsA(&gatewayA,
                             &ConnectionGatewayClient::ProtocolError);
  QSignalSpy protocolErrorsB(&gatewayB,
                             &ConnectionGatewayClient::ProtocolError);

  gatewayA.Open(sessionA);
  gatewayB.Open(sessionB);
  QTRY_COMPARE_WITH_TIMEOUT(authenticatedA.count(), 1, 10'000);
  QTRY_COMPARE_WITH_TIMEOUT(authenticatedB.count(), 1, 10'000);

  const QString friendsRequest = gatewayA.PullFriendList();
  QTRY_VERIFY_WITH_TIMEOUT(!responsesA.isEmpty() || !failuresA.isEmpty(),
                           10'000);
  QVERIFY(failuresA.isEmpty());
  const QList<QVariant> friendsResponse = responsesA.takeFirst();
  QCOMPARE(friendsResponse.at(0).toString(), friendsRequest);
  QCOMPARE(friendsResponse.at(1).toUInt(),
           quint32(protocol::PullFriendListResponse));
  wim::protocol::Packet friends;
  QVERIFY(ParsePacket(friendsResponse.at(2).toByteArray(), &friends));
  QCOMPARE(friends.has_error() ? friends.error() : 0, protocol::Success);
  bool foundPeer = false;
  for (const auto &item : friends.friend_list()) {
    foundPeer = foundPeer || item.uid() == sessionB.uid;
  }
  QVERIFY2(foundPeer, "live user B is missing from user A's friend list");

  responsesA.clear();
  const QString clientMessageId =
      QStringLiteral("qt-live-%1").arg(QDateTime::currentMSecsSinceEpoch());
  const QByteArray text = clientMessageId.toUtf8();
  const QString sendRequest =
      gatewayA.SendText(sessionB.uid, text, clientMessageId);
  QTRY_VERIFY_WITH_TIMEOUT(!responsesA.isEmpty() || !failuresA.isEmpty(),
                           10'000);
  QVERIFY(failuresA.isEmpty());
  const QList<QVariant> sendResponse = responsesA.takeFirst();
  QCOMPARE(sendResponse.at(0).toString(), sendRequest);
  QCOMPARE(sendResponse.at(1).toUInt(), quint32(protocol::SendTextResponse));
  wim::protocol::Packet accepted;
  QVERIFY(ParsePacket(sendResponse.at(2).toByteArray(), &accepted));
  QCOMPARE(accepted.has_error() ? accepted.error() : 0, protocol::Success);
  QVERIFY(accepted.message_id() > 0);
  QVERIFY(accepted.conversation_id() > 0);
  QVERIFY(accepted.conversation_seq() > 0);
  QCOMPARE(accepted.message_state(), wim::protocol::MESSAGE_STATE_ACCEPTED);
  QCOMPARE(QString::fromStdString(accepted.client_message_id()),
           clientMessageId);

  QTRY_VERIFY_WITH_TIMEOUT(!pushesB.isEmpty(), 10'000);
  const QList<QVariant> pushArguments = pushesB.takeFirst();
  QCOMPARE(pushArguments.at(0).toUInt(), quint32(protocol::SendTextRequest));
  wim::protocol::Packet pushed;
  QVERIFY(ParsePacket(pushArguments.at(1).toByteArray(), &pushed));
  QCOMPARE(pushed.message_id(), accepted.message_id());
  QCOMPARE(pushed.conversation_id(), accepted.conversation_id());
  QCOMPARE(pushed.conversation_seq(), accepted.conversation_seq());
  QCOMPARE(QString::fromStdString(pushed.client_message_id()), clientMessageId);
  QCOMPARE(QByteArray(pushed.data()), text);

  gatewayB.AcknowledgeTransport(pushed.message_id());
  gatewayB.AcknowledgeDelivered(pushed.message_id(), pushed.conversation_id(),
                                pushed.conversation_seq());
  gatewayB.AcknowledgeRead(pushed.message_id(), pushed.conversation_id(),
                           pushed.conversation_seq());

  responsesB.clear();
  const QString syncRequest = gatewayB.PullConversationMessages(
      pushed.conversation_id(), pushed.conversation_seq() - 1, 20);
  QTRY_VERIFY_WITH_TIMEOUT(!responsesB.isEmpty() || !failuresB.isEmpty(),
                           10'000);
  QVERIFY(failuresB.isEmpty());
  const QList<QVariant> syncResponse = responsesB.takeFirst();
  QCOMPARE(syncResponse.at(0).toString(), syncRequest);
  QCOMPARE(syncResponse.at(1).toUInt(),
           quint32(protocol::PullSessionMessagesResponse));
  wim::protocol::Packet synchronized;
  QVERIFY(ParsePacket(syncResponse.at(2).toByteArray(), &synchronized));
  QCOMPARE(synchronized.has_error() ? synchronized.error() : 0,
           protocol::Success);
  bool foundMessage = false;
  for (const auto &item : synchronized.message_list()) {
    foundMessage = foundMessage ||
                   (item.message_id() == pushed.message_id() &&
                    item.conversation_seq() == pushed.conversation_seq() &&
                    item.client_message_id() == pushed.client_message_id());
  }
  QVERIFY2(foundMessage, "accepted live message is missing from sync result");

  QTest::qWait(100);
  QCOMPARE(protocolErrorsA.count(), 0);
  QCOMPARE(protocolErrorsB.count(), 0);
  QCOMPARE(failuresA.count(), 0);
  QCOMPARE(failuresB.count(), 0);

  gatewayA.Close();
  gatewayB.Close();
  QTRY_COMPARE_WITH_TIMEOUT(gatewayA.CurrentState(),
                            ConnectionGatewayClient::State::Disconnected,
                            3'000);
  QTRY_COMPARE_WITH_TIMEOUT(gatewayB.CurrentState(),
                            ConnectionGatewayClient::State::Disconnected,
                            3'000);
}

}  // namespace wim::client

QTEST_GUILESS_MAIN(wim::client::ClientLiveNetworkTest)

#include "ClientLiveNetworkTest.moc"
