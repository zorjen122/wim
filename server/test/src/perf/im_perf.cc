#include "Const.h"
#include "TcpMessageCodec.h"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace wim::perf {
using boost::asio::ip::tcp;
using Clock = std::chrono::steady_clock;

struct Endpoint {
  std::string host{"127.0.0.1"};
  uint16_t port{8090};

  std::string toString() const {
    return host + ":" + std::to_string(port);
  }
};

struct Options {
  std::string mode{"basic"};
  std::vector<Endpoint> endpoints{{"127.0.0.1", 8090}};
  std::vector<Endpoint> receiverEndpoints{};
  int connections{16};
  int requestsPerConnection{100};
  int receivers{0};
  int receiverDrainMs{3000};
  int timeoutMs{5000};
  int payloadBytes{64};
  int64_t fromBase{500000};
  int64_t toBase{600000};
};

struct PacketResult {
  uint32_t serviceId{0};
  TcpPacket packet{};
  std::size_t bytes{0};
};

struct Stats {
  uint64_t attempted{0};
  uint64_t succeeded{0};
  uint64_t failed{0};
  uint64_t txBytes{0};
  uint64_t rxBytes{0};
  std::vector<double> rttUs{};

  void merge(const Stats &other) {
    attempted += other.attempted;
    succeeded += other.succeeded;
    failed += other.failed;
    txBytes += other.txBytes;
    rxBytes += other.rxBytes;
    rttUs.insert(rttUs.end(), other.rttUs.begin(), other.rttUs.end());
  }
};

// Splits a comma-style CLI value and ignores empty items.
std::vector<std::string> split(const std::string &value, char sep) {
  std::vector<std::string> parts;
  std::stringstream ss(value);
  std::string item;
  while (std::getline(ss, item, sep)) {
    if (!item.empty()) {
      parts.push_back(item);
    }
  }
  return parts;
}

// Parses one host:port endpoint from the command line.
Endpoint parseEndpoint(const std::string &value) {
  auto pos = value.rfind(':');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= value.size()) {
    throw std::runtime_error("invalid endpoint: " + value);
  }
  int port = std::stoi(value.substr(pos + 1));
  if (port <= 0 || port > 65535) {
    throw std::runtime_error("invalid endpoint port: " + value);
  }
  return Endpoint{value.substr(0, pos), static_cast<uint16_t>(port)};
}

// Parses a comma-separated endpoint list.
std::vector<Endpoint> parseEndpoints(const std::string &value) {
  std::vector<Endpoint> endpoints;
  for (const auto &item : split(value, ',')) {
    endpoints.push_back(parseEndpoint(item));
  }
  if (endpoints.empty()) {
    throw std::runtime_error("endpoint list is empty");
  }
  return endpoints;
}

// Prints the supported perf modes and CLI options.
void printUsage(const char *program) {
  std::cout
      << "Usage: " << program << " [options]\n\n"
      << "Modes:\n"
      << "  --mode basic  Test ChatServer/ChatSession roundtrip with login + "
         "ping.\n"
      << "  --mode text   Test ID_TEXT_SEND_REQ application requests.\n\n"
      << "Options:\n"
      << "  --endpoints host:port[,host:port]          Sender endpoints. "
         "Default: 127.0.0.1:8090\n"
      << "  --receiver-endpoints host:port[,host:port] Receiver endpoints "
         "for online text targets.\n"
      << "  --connections N                            Concurrent sender "
         "connections. Default: 16\n"
      << "  --requests N                               Requests per sender. "
         "Default: 100\n"
      << "  --receivers N                              Online receivers for "
         "text mode. Default: 0\n"
      << "  --receiver-drain-ms N                     Wait for async pushes "
         "after senders finish. Default: 3000\n"
      << "  --payload-bytes N                          Text payload size. "
         "Default: 64\n"
      << "  --from-base UID                            First sender UID. "
         "Default: 500000\n"
      << "  --to-base UID                              First receiver UID. "
         "Default: 600000\n"
      << "  --timeout-ms N                             Per request timeout. "
         "Default: 5000\n\n"
      << "Cluster text example:\n"
      << "  " << program
      << " --mode text --endpoints 127.0.0.1:8090,127.0.0.1:8091 "
         "--receiver-endpoints 127.0.0.1:8091,127.0.0.1:8090 "
         "--connections 32 --receivers 32 --requests 100\n";
}

// Converts argv into runtime options and validates unsafe combinations.
Options parseOptions(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto requireValue = [&](const std::string &name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + name);
      }
      return argv[++i];
    };

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      std::exit(0);
    } else if (arg == "--mode") {
      options.mode = requireValue(arg);
    } else if (arg == "--endpoints") {
      options.endpoints = parseEndpoints(requireValue(arg));
    } else if (arg == "--receiver-endpoints") {
      options.receiverEndpoints = parseEndpoints(requireValue(arg));
    } else if (arg == "--connections") {
      options.connections = std::stoi(requireValue(arg));
    } else if (arg == "--requests") {
      options.requestsPerConnection = std::stoi(requireValue(arg));
    } else if (arg == "--receivers") {
      options.receivers = std::stoi(requireValue(arg));
    } else if (arg == "--receiver-drain-ms") {
      options.receiverDrainMs = std::stoi(requireValue(arg));
    } else if (arg == "--payload-bytes") {
      options.payloadBytes = std::stoi(requireValue(arg));
    } else if (arg == "--from-base") {
      options.fromBase = std::stoll(requireValue(arg));
    } else if (arg == "--to-base") {
      options.toBase = std::stoll(requireValue(arg));
    } else if (arg == "--timeout-ms") {
      options.timeoutMs = std::stoi(requireValue(arg));
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }

  if (options.mode != "basic" && options.mode != "text") {
    throw std::runtime_error("--mode must be basic or text");
  }
  if (options.connections <= 0 || options.requestsPerConnection <= 0) {
    throw std::runtime_error("--connections and --requests must be positive");
  }
  if (options.receivers < 0 || options.receiverDrainMs < 0 ||
      options.payloadBytes < 0 || options.timeoutMs <= 0) {
    throw std::runtime_error(
        "--receivers/--payload-bytes/--timeout-ms invalid");
  }
  if (options.receiverEndpoints.empty()) {
    options.receiverEndpoints = options.endpoints;
  }
  return options;
}

// Small blocking TCP client for the WIM packet protocol.
class SyncClient {
 public:
  SyncClient(const Endpoint &endpoint, int timeoutMs)
      : endpoint(endpoint), socket(ioContext), timeoutMs(timeoutMs) {}

  // Opens a TCP connection and switches reads to non-blocking polling.
  void connect() {
    tcp::resolver resolver(ioContext);
    auto results =
        resolver.resolve(endpoint.host, std::to_string(endpoint.port));
    boost::asio::connect(socket, results);
    socket.set_option(tcp::no_delay(true));
    socket.non_blocking(true);
  }

  // Closes the client socket without surfacing cleanup errors.
  void close() {
    boost::system::error_code ec;
    socket.close(ec);
  }

  // Logs in an existing perf user, creating its userInfo row if needed.
  bool login(int64_t uid) {
    TcpPacket request;
    request.set_uid(uid);
    request.set_init(false);
    auto response =
        requestPacket(ID_LOGIN_INIT_REQ, request, ID_LOGIN_INIT_RSP, timeoutMs);
    if (response.has_value() &&
        response->packet.error() == ErrorCodes::Success) {
      return true;
    }

    TcpPacket initRequest;
    initRequest.set_uid(uid);
    initRequest.set_init(true);
    initRequest.set_name("perf-" + std::to_string(uid));
    initRequest.set_age(18);
    initRequest.set_sex("perf");
    response = requestPacket(ID_LOGIN_INIT_REQ, initRequest, ID_LOGIN_INIT_RSP,
                             timeoutMs);
    return response.has_value() &&
           response->packet.error() == ErrorCodes::Success;
  }

  // Sends the logical quit message used by chat service cleanup.
  void quit(int64_t uid) {
    TcpPacket request;
    request.set_uid(uid);
    sendPacket(ID_USER_QUIT_REQ, request);
  }

  // Acknowledges an asynchronously pushed text message.
  void ack(int64_t uid, int64_t seq) {
    TcpPacket request;
    request.set_uid(uid);
    request.set_seq(seq);
    sendPacket(ID_ACK, request);
  }

  // Serializes and writes one protocol packet to the socket.
  void sendPacket(uint32_t serviceId, const TcpPacket &packet) {
    std::string body = SerializeTcpPacket(packet);
    uint32_t netId = htonl(serviceId);
    uint32_t netLen = htonl(static_cast<uint32_t>(body.size()));
    std::array<boost::asio::const_buffer, 3> buffers{
        boost::asio::buffer(&netId, sizeof(netId)),
        boost::asio::buffer(&netLen, sizeof(netLen)),
        boost::asio::buffer(body)};
    socket.non_blocking(false);
    boost::asio::write(socket, buffers);
    socket.non_blocking(true);
    txBytes += PROTOCOL_HEADER_TOTAL + body.size();
  }

  // Polls until exactly size bytes are read or the timeout expires.
  bool readExact(void *data, std::size_t size, int timeout) {
    auto deadline = Clock::now() + std::chrono::milliseconds(timeout);
    auto *cursor = static_cast<char *>(data);
    std::size_t remaining = size;

    while (remaining > 0) {
      boost::system::error_code ec;
      std::size_t bytes =
          socket.read_some(boost::asio::buffer(cursor, remaining), ec);
      if (!ec) {
        cursor += bytes;
        remaining -= bytes;
        continue;
      }
      if (ec == boost::asio::error::would_block ||
          ec == boost::asio::error::try_again) {
        if (Clock::now() >= deadline) {
          return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      if (ec == boost::asio::error::eof ||
          ec == boost::asio::error::connection_reset) {
        return false;
      }
      throw boost::system::system_error(ec);
    }
    return true;
  }

  // Reads and parses one packet, returning nullopt on timeout/close.
  std::optional<PacketResult> recvPacket(int timeout) {
    uint32_t netId = 0;
    uint32_t netLen = 0;
    if (!readExact(&netId, sizeof(netId), timeout) ||
        !readExact(&netLen, sizeof(netLen), timeout)) {
      return std::nullopt;
    }
    uint32_t serviceId = ntohl(netId);
    uint32_t bodyLen = ntohl(netLen);
    if (bodyLen > PROTOCOL_RECV_MSS) {
      throw std::runtime_error("response body too large");
    }
    std::string body(bodyLen, '\0');
    if (bodyLen > 0 && !readExact(body.data(), body.size(), timeout)) {
      return std::nullopt;
    }
    TcpPacket packet;
    if (bodyLen > 0 && !ParseTcpPacket(body, packet)) {
      throw std::runtime_error("failed to parse protobuf response");
    }
    rxBytes += PROTOCOL_HEADER_TOTAL + bodyLen;
    return PacketResult{serviceId, packet, PROTOCOL_HEADER_TOTAL + bodyLen};
  }

  // Sends a request and waits for the expected response packet.
  std::optional<PacketResult> requestPacket(uint32_t serviceId,
                                            const TcpPacket &packet,
                                            uint32_t expectedId,
                                            int timeoutMs) {
    sendPacket(serviceId, packet);
    auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
      auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                           deadline - Clock::now())
                           .count();
      auto result =
          recvPacket(static_cast<int>(std::max<int64_t>(1, remaining)));
      if (!result.has_value()) {
        continue;
      }
      if (result->serviceId == expectedId) {
        return result;
      }
      if (result->serviceId == ID_TEXT_SEND_REQ && result->packet.has_seq()) {
        ack(packet.uid(), result->packet.seq());
      }
    }
    return std::nullopt;
  }

  uint64_t tx() const {
    return txBytes;
  }

  uint64_t rx() const {
    return rxBytes;
  }

 private:
  Endpoint endpoint;
  boost::asio::io_context ioContext;
  tcp::socket socket;
  int timeoutMs;
  uint64_t txBytes{0};
  uint64_t rxBytes{0};
};

// Builds a deterministic text payload for text-message tests.
std::string makePayload(int size) {
  std::string payload;
  payload.reserve(size);
  for (int i = 0; i < size; ++i) {
    payload.push_back(static_cast<char>('a' + (i % 26)));
  }
  return payload;
}

// Checks service-level success and an optional sequence echo.
bool isSuccessResponse(const PacketResult &result, int64_t expectedSeq = 0) {
  if (!result.packet.has_error() ||
      result.packet.error() != ErrorCodes::Success) {
    return false;
  }
  if (expectedSeq != 0 && result.packet.has_seq() &&
      result.packet.seq() != expectedSeq) {
    return false;
  }
  return true;
}

// Keeps one receiver user online and ACKs incoming pushed text messages.
void receiverWorker(const Options &options, int index, std::atomic<bool> &stop,
                    std::atomic<int> &ready, std::atomic<uint64_t> &received,
                    std::atomic<uint64_t> &receiverBytes) {
  int64_t uid = options.toBase + index;
  const Endpoint &endpoint =
      options.receiverEndpoints[index % options.receiverEndpoints.size()];
  try {
    SyncClient client(endpoint, 250);
    client.connect();
    if (!client.login(uid)) {
      std::cerr << "receiver login failed uid=" << uid
                << " endpoint=" << endpoint.toString() << "\n";
      ready.fetch_add(1);
      return;
    }
    ready.fetch_add(1);

    while (!stop.load()) {
      auto packet = client.recvPacket(250);
      if (!packet.has_value()) {
        continue;
      }
      if (packet->serviceId == ID_TEXT_SEND_REQ && packet->packet.has_seq()) {
        client.ack(uid, packet->packet.seq());
        received.fetch_add(1);
      }
    }
    client.quit(uid);
    receiverBytes.fetch_add(client.tx() + client.rx());
  } catch (const std::exception &error) {
    ready.fetch_add(1);
    std::cerr << "receiver error uid=" << uid << ": " << error.what() << "\n";
  }
}

// Runs one sender connection and records request latency statistics.
Stats senderWorker(const Options &options, int index,
                   const std::atomic<bool> &startFlag) {
  Stats stats;
  int64_t uid = options.fromBase + index;
  const Endpoint &endpoint =
      options.endpoints[index % options.endpoints.size()];
  std::string payload = makePayload(options.payloadBytes);

  try {
    SyncClient client(endpoint, options.timeoutMs);
    client.connect();
    if (!client.login(uid)) {
      stats.failed += options.requestsPerConnection;
      stats.attempted += options.requestsPerConnection;
      return stats;
    }

    while (!startFlag.load()) {
      std::this_thread::yield();
    }

    for (int i = 0; i < options.requestsPerConnection; ++i) {
      stats.attempted += 1;
      TcpPacket request;
      uint32_t serviceId = ID_PING_REQ;
      uint32_t expectedId = ID_PING_RSP;
      int64_t expectedSeq = 0;

      if (options.mode == "basic") {
        request.set_uid(uid);
      } else {
        expectedSeq =
            (static_cast<int64_t>(index) << 32) + static_cast<int64_t>(i + 1);
        int64_t target = options.toBase;
        if (options.receivers > 0) {
          target +=
              (index * options.requestsPerConnection + i) % options.receivers;
        }
        request.set_seq(expectedSeq);
        request.set_from(uid);
        request.set_to(target);
        request.set_session_key(0);
        request.set_data(payload);
        serviceId = ID_TEXT_SEND_REQ;
        expectedId = ID_TEXT_SEND_RSP;
      }

      auto begin = Clock::now();
      auto result = client.requestPacket(serviceId, request, expectedId,
                                         options.timeoutMs);
      auto end = Clock::now();

      if (result.has_value() && isSuccessResponse(*result, expectedSeq)) {
        stats.succeeded += 1;
        stats.rttUs.push_back(
            std::chrono::duration<double, std::micro>(end - begin).count());
      } else {
        stats.failed += 1;
      }
    }

    client.quit(uid);
    stats.txBytes += client.tx();
    stats.rxBytes += client.rx();
  } catch (const std::exception &error) {
    stats.failed += options.requestsPerConnection - stats.attempted;
    stats.attempted = options.requestsPerConnection;
    std::cerr << "sender error uid=" << uid
              << " endpoint=" << endpoint.toString() << ": " << error.what()
              << "\n";
  }
  return stats;
}

// Computes an interpolated percentile from sorted latency samples.
double percentile(const std::vector<double> &values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  double rank = (p / 100.0) * static_cast<double>(values.size() - 1);
  auto lower = static_cast<std::size_t>(rank);
  auto upper = std::min(lower + 1, values.size() - 1);
  double weight = rank - static_cast<double>(lower);
  return values[lower] * (1.0 - weight) + values[upper] * weight;
}

// Prints the final throughput, byte counters, and latency percentiles.
void printReport(const Options &options, Stats stats,
                 std::chrono::duration<double> elapsed,
                 uint64_t receiverMessages, uint64_t receiverBytes) {
  std::sort(stats.rttUs.begin(), stats.rttUs.end());
  double elapsedSeconds = elapsed.count();
  double successRate = stats.attempted == 0
                           ? 0.0
                           : (static_cast<double>(stats.succeeded) * 100.0 /
                              static_cast<double>(stats.attempted));
  double avgUs =
      stats.rttUs.empty()
          ? 0.0
          : std::accumulate(stats.rttUs.begin(), stats.rttUs.end(), 0.0) /
                static_cast<double>(stats.rttUs.size());
  uint64_t totalBytes = stats.txBytes + stats.rxBytes + receiverBytes;

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "mode: " << options.mode << "\n";
  std::cout << "endpoints: ";
  for (std::size_t i = 0; i < options.endpoints.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << options.endpoints[i].toString();
  }
  std::cout << "\n";
  std::cout << "connections: " << options.connections << "\n";
  std::cout << "requests_per_connection: " << options.requestsPerConnection
            << "\n";
  std::cout << "attempted: " << stats.attempted << "\n";
  std::cout << "succeeded: " << stats.succeeded << "\n";
  std::cout << "failed: " << stats.failed << "\n";
  std::cout << "success_rate: " << successRate << "%\n";
  std::cout << "elapsed_seconds: " << elapsedSeconds << "\n";
  std::cout << "throughput_success_rps: "
            << (elapsedSeconds > 0.0 ? stats.succeeded / elapsedSeconds : 0.0)
            << "\n";
  std::cout << "throughput_attempt_rps: "
            << (elapsedSeconds > 0.0 ? stats.attempted / elapsedSeconds : 0.0)
            << "\n";
  std::cout << "bandwidth_mib_per_sec: "
            << (elapsedSeconds > 0.0
                    ? (static_cast<double>(totalBytes) / 1024.0 / 1024.0) /
                          elapsedSeconds
                    : 0.0)
            << "\n";
  std::cout << "tx_bytes: " << stats.txBytes << "\n";
  std::cout << "rx_bytes: " << stats.rxBytes << "\n";
  if (options.receivers > 0) {
    std::cout << "receiver_push_messages: " << receiverMessages << "\n";
    std::cout << "receiver_bytes: " << receiverBytes << "\n";
  }
  std::cout << "rtt_min_ms: "
            << (stats.rttUs.empty() ? 0.0 : stats.rttUs.front() / 1000.0)
            << "\n";
  std::cout << "rtt_avg_ms: " << avgUs / 1000.0 << "\n";
  std::cout << "rtt_p50_ms: " << percentile(stats.rttUs, 50.0) / 1000.0 << "\n";
  std::cout << "rtt_p95_ms: " << percentile(stats.rttUs, 95.0) / 1000.0 << "\n";
  std::cout << "rtt_p99_ms: " << percentile(stats.rttUs, 99.0) / 1000.0 << "\n";
  std::cout << "rtt_max_ms: "
            << (stats.rttUs.empty() ? 0.0 : stats.rttUs.back() / 1000.0)
            << "\n";
}

// Coordinates receiver setup, sender fan-out, draining, and reporting.
int run(int argc, char **argv) {
  Options options = parseOptions(argc, argv);

  std::atomic<bool> stopReceivers{false};
  std::atomic<int> readyReceivers{0};
  std::atomic<uint64_t> receiverMessages{0};
  std::atomic<uint64_t> receiverBytes{0};
  std::vector<std::thread> receiverThreads;

  if (options.mode == "text" && options.receivers > 0) {
    for (int i = 0; i < options.receivers; ++i) {
      receiverThreads.emplace_back(
          receiverWorker, std::cref(options), i, std::ref(stopReceivers),
          std::ref(readyReceivers), std::ref(receiverMessages),
          std::ref(receiverBytes));
    }
    while (readyReceivers.load() < options.receivers) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  std::atomic<bool> startFlag{false};
  std::mutex statsMutex;
  Stats totalStats;
  std::vector<std::thread> senderThreads;

  auto start = Clock::now();
  for (int i = 0; i < options.connections; ++i) {
    senderThreads.emplace_back([&, i]() {
      Stats local = senderWorker(options, i, startFlag);
      std::lock_guard<std::mutex> lock(statsMutex);
      totalStats.merge(local);
    });
  }

  start = Clock::now();
  startFlag = true;
  for (auto &thread : senderThreads) {
    thread.join();
  }
  auto end = Clock::now();

  if (options.receivers > 0 && options.receiverDrainMs > 0) {
    auto drainDeadline =
        Clock::now() + std::chrono::milliseconds(options.receiverDrainMs);
    while (Clock::now() < drainDeadline &&
           receiverMessages.load() < totalStats.succeeded) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  stopReceivers = true;
  for (auto &thread : receiverThreads) {
    thread.join();
  }

  printReport(options, totalStats, end - start, receiverMessages.load(),
              receiverBytes.load());
  return totalStats.failed == 0 ? 0 : 1;
}

}  // namespace wim::perf

// CLI entrypoint that maps exceptions to a stable non-zero exit code.
int main(int argc, char **argv) {
  try {
    return wim::perf::run(argc, argv);
  } catch (const std::exception &error) {
    std::cerr << "imPerf: " << error.what() << "\n";
    return 2;
  }
}
