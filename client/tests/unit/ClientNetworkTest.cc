#include "adapters/connection_gateway/ClientProtocol.h"
#include "adapters/connection_gateway/ConnectionGatewayClient.h"
#include "adapters/connection_gateway/TcpFrameCodec.h"
#include "adapters/gate/GateHttpClient.h"
#include "tcp_message.pb.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

namespace wim::client {
namespace {

wim::protocol::Packet ParsePacket(const QByteArray &payload) {
  wim::protocol::Packet packet;
  const bool parsed =
      packet.ParseFromArray(payload.constData(), payload.size());
  Q_ASSERT(parsed);
  return packet;
}

QByteArray PacketPayload(const wim::protocol::Packet &packet) {
  std::string payload;
  const bool serialized = packet.SerializeToString(&payload);
  Q_ASSERT(serialized);
  return QByteArray::fromStdString(payload);
}

void WriteHttpJson(QTcpSocket *socket, const QJsonObject &object) {
  const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
  const QByteArray response =
      QByteArrayLiteral(
          "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n") +
      QByteArrayLiteral("Connection: close\r\nContent-Length: ") +
      QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
  socket->write(response);
  socket->disconnectFromHost();
}

}  // namespace

class ClientNetworkTest final : public QObject {
  Q_OBJECT

 private slots:
  void gateSignInParsesGatewaySession();
  void gateCoversAccountRequests();
  void gatewayCoversSupportedRequestAndReceiptContracts();
  void gatewayReconnectsAndAuthenticatesAgain();
};

void ClientNetworkTest::gateSignInParsesGatewaySession() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));

  QByteArray receivedRequest;
  connect(&server, &QTcpServer::newConnection, this, [&] {
    auto *socket = server.nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, socket, [socket, &receivedRequest] {
      receivedRequest += socket->readAll();
      const int headerEnd = receivedRequest.indexOf("\r\n\r\n");
      if (headerEnd < 0 ||
          !receivedRequest.contains(QByteArrayLiteral("zongjing"))) {
        return;
      }
      WriteHttpJson(socket,
                    {{QStringLiteral("error"), 0},
                     {QStringLiteral("uid"), 9001},
                     {QStringLiteral("ip"), QStringLiteral("127.0.0.1")},
                     {QStringLiteral("port"), 19090},
                     {QStringLiteral("gatewayId"), QStringLiteral("gateway-a")},
                     {QStringLiteral("chatToken"), QStringLiteral("secret")},
                     {QStringLiteral("chatTokenExpiresIn"), 900},
                     {QStringLiteral("init"), 1}});
    });
  });

  GateHttpClient gate;
  gate.SetBaseUrl(
      QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
  QSignalSpy success(&gate, &GateHttpClient::SignInSucceeded);
  QSignalSpy failure(&gate, &GateHttpClient::OperationFailed);
  gate.SignIn(QStringLiteral("zongjing"), QStringLiteral("password"));

  QTRY_COMPARE(success.count(), 1);
  QCOMPARE(failure.count(), 0);
  QVERIFY(receivedRequest.startsWith(QByteArrayLiteral("POST /post-signIn")));
  QVERIFY(
      receivedRequest.contains(QByteArrayLiteral("\"password\":\"password\"")));

  const GateSession session =
      qvariant_cast<GateSession>(success.takeFirst().at(0));
  QCOMPARE(session.uid, 9001);
  QCOMPARE(session.gatewayHost, QStringLiteral("127.0.0.1"));
  QCOMPARE(session.gatewayPort, quint16(19090));
  QCOMPARE(session.gatewayId, QStringLiteral("gateway-a"));
  QCOMPARE(session.token, QStringLiteral("secret"));
  QCOMPARE(session.tokenExpiresInSeconds, 900);
  QVERIFY(session.profileInitializationRequired);
}

void ClientNetworkTest::gateCoversAccountRequests() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QList<QByteArray> requests;

  connect(&server, &QTcpServer::newConnection, this, [&] {
    auto *socket = server.nextPendingConnection();
    auto received = QSharedPointer<QByteArray>::create();
    connect(socket, &QTcpSocket::readyRead, socket,
            [socket, received, &requests] {
              *received += socket->readAll();
              if (!received->contains(QByteArrayLiteral("\r\n\r\n")) ||
                  !received->trimmed().endsWith('}')) {
                return;
              }
              requests.push_back(*received);
              WriteHttpJson(socket, {{QStringLiteral("error"), 0}});
            });
  });

  GateHttpClient gate;
  gate.SetBaseUrl(
      QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
  QSignalSpy succeeded(&gate, &GateHttpClient::OperationSucceeded);
  QSignalSpy failed(&gate, &GateHttpClient::OperationFailed);

  gate.RequestVerificationCode(QStringLiteral("user@example.com"));
  QTRY_COMPARE(succeeded.count(), 1);
  gate.SignUp(QStringLiteral("user"), QStringLiteral("password"),
              QStringLiteral("user@example.com"), QStringLiteral("1234"));
  QTRY_COMPARE(succeeded.count(), 2);
  gate.ForgetPassword(QStringLiteral("user"),
                      QStringLiteral("user@example.com"),
                      QStringLiteral("1234"), QStringLiteral("new-password"));
  QTRY_COMPARE(succeeded.count(), 3);

  QCOMPARE(failed.count(), 0);
  QCOMPARE(requests.size(), 3);
  QVERIFY(requests[0].startsWith(QByteArrayLiteral("POST /post-verifycode")));
  QVERIFY(requests[0].contains(QByteArrayLiteral("user@example.com")));
  QVERIFY(requests[1].startsWith(QByteArrayLiteral("POST /post-signUp")));
  QVERIFY(requests[1].contains(QByteArrayLiteral("\"verifycode\":\"1234\"")));
  QVERIFY(
      requests[2].startsWith(QByteArrayLiteral("POST /post-forget-password")));
  QVERIFY(requests[2].contains(QByteArrayLiteral("new-password")));
}

void ClientNetworkTest::gatewayCoversSupportedRequestAndReceiptContracts() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));

  QTcpSocket *serverSocket = nullptr;
  TcpFrameCodec serverCodec;
  QVector<quint32> requestServices;
  QHash<quint32, wim::protocol::Packet> packets;
  QVector<wim::protocol::Packet> receipts;

  connect(&server, &QTcpServer::newConnection, this, [&] {
    serverSocket = server.nextPendingConnection();
    connect(serverSocket, &QTcpSocket::readyRead, serverSocket, [&] {
      const auto frames = serverCodec.Feed(serverSocket->readAll());
      for (const auto &frame : frames) {
        const auto packet = ParsePacket(frame.payload);
        if (frame.serviceId == protocol::LoginRequest) {
          QCOMPARE(packet.uid(), 42);
          QCOMPARE(QString::fromStdString(packet.auth_token()),
                   QStringLiteral("token-42"));
          wim::protocol::Packet response;
          response.set_error(protocol::Success);
          response.set_uid(42);
          serverSocket->write(TcpFrameCodec::Encode(protocol::LoginResponse,
                                                    PacketPayload(response)));
          continue;
        }
        if (frame.serviceId == protocol::QuitRequest) {
          wim::protocol::Packet response;
          response.set_error(protocol::Success);
          serverSocket->write(TcpFrameCodec::Encode(protocol::QuitResponse,
                                                    PacketPayload(response)));
          continue;
        }
        if (frame.serviceId == protocol::Ack) {
          receipts.push_back(packet);
          continue;
        }
        requestServices.push_back(frame.serviceId);
        packets.insert(frame.serviceId, packet);
        wim::protocol::Packet response;
        response.set_error(protocol::Success);
        if (frame.serviceId == protocol::SendTextRequest ||
            frame.serviceId == protocol::SendGroupTextRequest) {
          response.set_client_message_id(packet.client_message_id());
          response.set_message_id(7001);
          response.set_conversation_id(8001);
          response.set_conversation_seq(9);
          response.set_message_state(wim::protocol::MESSAGE_STATE_ACCEPTED);
        }
        serverSocket->write(TcpFrameCodec::Encode(
            protocol::ResponseFor(frame.serviceId), PacketPayload(response)));
      }
    });
  });

  ConnectionGatewayClient gateway;
  QSignalSpy authenticated(&gateway, &ConnectionGatewayClient::Authenticated);
  QSignalSpy responses(&gateway, &ConnectionGatewayClient::ResponseReceived);
  QSignalSpy pushes(&gateway, &ConnectionGatewayClient::PushReceived);
  QSignalSpy failures(&gateway, &ConnectionGatewayClient::RequestFailed);
  gateway.Open(GateSession{
      .uid = 42,
      .gatewayHost = QStringLiteral("127.0.0.1"),
      .gatewayPort = server.serverPort(),
      .gatewayId = QStringLiteral("local-gateway"),
      .token = QStringLiteral("token-42"),
      .tokenExpiresInSeconds = 900,
  });
  QTRY_COMPARE(authenticated.count(), 1);

  gateway.PullFriendList();
  gateway.PullFriendList();
  gateway.PullFriendApplications();
  gateway.PullConversationMessages(8001, 7, 60);
  gateway.PullAllMessages(99, 40);
  gateway.SendFriendRequest(43, QStringLiteral("hello"));
  gateway.ReplyFriendRequest(43, true, QStringLiteral("welcome"));
  gateway.SendText(43, QByteArrayLiteral("text"), QStringLiteral("client-text"),
                   8001);
  gateway.UploadFile(100, QStringLiteral("note.txt"), QStringLiteral("TEXT"),
                     QByteArrayLiteral("file"));
  gateway.CreateGroup(QStringLiteral("Qt group"));
  gateway.RequestJoinGroup(5001, QStringLiteral("join"));
  gateway.ReplyJoinGroup(5001, 43, true);
  gateway.SendGroupText(5001, QByteArrayLiteral("group text"),
                        QStringLiteral("client-group"), 8100);

  QTRY_COMPARE(responses.count(), 13);
  QCOMPARE(failures.count(), 0);
  const QVector<quint32> expectedServices = {
      protocol::PullFriendListRequest,
      protocol::PullFriendApplyListRequest,
      protocol::PullSessionMessagesRequest,
      protocol::PullMessagesRequest,
      protocol::AddFriendRequest,
      protocol::ReplyFriendRequest,
      protocol::SendTextRequest,
      protocol::UploadFileRequest,
      protocol::CreateGroupRequest,
      protocol::JoinGroupRequest,
      protocol::ReplyJoinGroupRequest,
      protocol::SendGroupTextRequest,
  };
  for (const quint32 service : expectedServices) {
    QVERIFY2(
        requestServices.contains(service),
        qPrintable(QStringLiteral("missing request service %1").arg(service)));
  }
  QCOMPARE(requestServices.count(protocol::PullFriendListRequest), 2);

  QCOMPARE(packets[protocol::PullSessionMessagesRequest].conversation_id(),
           8001);
  QCOMPARE(packets[protocol::PullSessionMessagesRequest].after_seq(), 7);
  QCOMPARE(packets[protocol::SendTextRequest].to(), 43);
  QCOMPARE(QString::fromStdString(
               packets[protocol::SendTextRequest].client_message_id()),
           QStringLiteral("client-text"));
  QCOMPARE(packets[protocol::SendGroupTextRequest].gid(), 5001);
  QCOMPARE(
      QString::fromStdString(packets[protocol::UploadFileRequest].file_type()),
      QStringLiteral("TEXT"));
  for (const quint32 service : expectedServices) {
    QVERIFY(!QString::fromStdString(packets[service].request_id()).isEmpty());
    QCOMPARE(packets[service].request_timeout_ms(), 3000U);
  }

  wim::protocol::Packet push;
  push.set_from(43);
  push.set_to(42);
  push.set_data("incoming");
  push.set_seq(7002);
  push.set_message_id(7002);
  push.set_conversation_id(8001);
  push.set_conversation_seq(10);
  serverSocket->write(
      TcpFrameCodec::Encode(protocol::SendTextRequest, PacketPayload(push)));
  QTRY_COMPARE(pushes.count(), 1);

  gateway.AcknowledgeTransport(7002);
  gateway.AcknowledgeDelivered(7002, 8001, 10);
  gateway.AcknowledgeRead(7002, 8001, 10);
  QTRY_COMPARE(receipts.size(), 3);
  QCOMPARE(receipts[0].receipt_type(), wim::protocol::RECEIPT_TYPE_TRANSPORT);
  QVERIFY(!receipts[0].has_conversation_id());
  QCOMPARE(receipts[1].receipt_type(), wim::protocol::RECEIPT_TYPE_DELIVERED);
  QCOMPARE(receipts[1].conversation_id(), 8001);
  QCOMPARE(receipts[1].conversation_seq(), 10);
  QCOMPARE(receipts[2].receipt_type(), wim::protocol::RECEIPT_TYPE_READ);

  gateway.Close();
  QTRY_COMPARE(gateway.CurrentState(),
               ConnectionGatewayClient::State::Disconnected);
}

void ClientNetworkTest::gatewayReconnectsAndAuthenticatesAgain() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  int acceptedConnections = 0;

  connect(&server, &QTcpServer::newConnection, this, [&] {
    auto *socket = server.nextPendingConnection();
    ++acceptedConnections;
    auto codec = QSharedPointer<TcpFrameCodec>::create();
    connect(socket, &QTcpSocket::readyRead, socket, [socket, codec] {
      for (const auto &frame : codec->Feed(socket->readAll())) {
        if (frame.serviceId != protocol::LoginRequest) {
          continue;
        }
        wim::protocol::Packet response;
        response.set_error(protocol::Success);
        socket->write(TcpFrameCodec::Encode(protocol::LoginResponse,
                                            PacketPayload(response)));
      }
    });
  });

  ConnectionGatewayClient gateway;
  QSignalSpy authenticated(&gateway, &ConnectionGatewayClient::Authenticated);
  gateway.Open(GateSession{
      .uid = 55,
      .gatewayHost = QStringLiteral("127.0.0.1"),
      .gatewayPort = server.serverPort(),
      .token = QStringLiteral("token-55"),
      .tokenExpiresInSeconds = 900,
  });
  QTRY_COMPARE(authenticated.count(), 1);
  QCOMPARE(acceptedConnections, 1);

  const auto sockets = server.findChildren<QTcpSocket *>();
  QVERIFY(!sockets.isEmpty());
  sockets.front()->disconnectFromHost();
  QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 2, 3000);
  QCOMPARE(acceptedConnections, 2);
  gateway.Close();
}

}  // namespace wim::client

QTEST_GUILESS_MAIN(wim::client::ClientNetworkTest)

#include "ClientNetworkTest.moc"
