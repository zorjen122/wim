# wim Local Build Requirements

## System Packages

Tested on Ubuntu 26.04 with clang.

| Component | Used by | Packages / runtime |
| --- | --- | --- |
| C++ toolchain | all C++ services | `clang`, `cmake`, `build-essential`, `pkg-config` |
| Protobuf / gRPC | gate, state, chat, file, gateway, protocol smoke scripts | `protobuf-compiler`, `protobuf-compiler-grpc`, `libprotobuf-dev`, `libgrpc++-dev`, `libgrpc-dev`, `python3-protobuf` |
| Qt client | Linux desktop client and QtProtobuf code generation | `qt6-base-dev`, `qt6-declarative-dev`, `qt6-svg-dev`, `qt6-grpc-dev` |
| Boost.Asio / Beast | gate, chat, test client | `libboost-dev`, `libboost-filesystem-dev` |
| Config / logging / JSON | all C++ services | `libyaml-cpp-dev`, `libspdlog-dev`, `libfmt-dev`, `libjsoncpp-dev` |
| Redis | verify codes, authentication handoff, online routing, IDs, bounded text retry deduplication | `redis-server`, `libhiredis-dev` |
| MySQL | users, relationships, and messages | `mysql-server`, MySQL Connector/C++ X DevAPI packages |
| Kafka placeholder | chat utility code links librdkafka | `librdkafka-dev` |
| SMTP client | Gate email verification delivery | `libcurl4-openssl-dev` |

Install from Ubuntu repositories:

```bash
sudo apt-get install -y \
  clang cmake build-essential pkg-config \
  protobuf-compiler protobuf-compiler-grpc libprotobuf-dev python3-protobuf \
  libgrpc++-dev libgrpc-dev \
  qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-grpc-dev \
  libboost-dev libboost-filesystem-dev \
  libyaml-cpp-dev libspdlog-dev libfmt-dev libjsoncpp-dev \
  libhiredis-dev redis-server mysql-server libcurl4-openssl-dev \
  librdkafka-dev
```

Install MySQL Connector/C++ from the MySQL APT repository:

```bash
curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2025 -o /tmp/RPM-GPG-KEY-mysql-2025
sudo sh -c 'gpg --dearmor < /tmp/RPM-GPG-KEY-mysql-2025 > /usr/share/keyrings/mysql-apt-config.gpg'
sudo apt-get update
sudo apt-get install -y libmysqlcppconn-dev libmysqlcppconn10 libmysqlcppconnx2
```

The code uses MySQL Connector/C++ X DevAPI, so MySQL X Plugin must listen on `127.0.0.1:33060`.

## Local Ports

- `18080`: gate HTTP API
- `50052`: state gRPC
- `50055`: primary Message gRPC stream endpoint
- `50056`: secondary Message gRPC stream endpoint
- `8090`: primary Connection Gateway TCP
- `8091`: secondary Connection Gateway TCP
- `51000`: file gRPC
- `6380`: project Redis, password `root`
- `33060`: MySQL X Plugin

## Build

```bash
./scripts/build.sh
```

The build generates gRPC/protobuf C++ sources into the CMake build directory.

## Initialize Data

```bash
./scripts/init_mysql.sh
./scripts/start_redis.sh
```

`init_mysql.sh` creates the `zorjen/root` MySQL user and imports `server/chat/init_mysql_test.sql`.

## Run Services

```bash
./scripts/run_local_services.sh
```

For local smoke tests, Gate stores the code in Redis and includes it in the HTTP
response because `gate.yaml` enables `exposeCodeInResponse`. Production should
disable that flag and enable SMTP delivery.

To run two chat nodes for cross-node forwarding:

```bash
WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
WIM_GATEWAY_CONFIGS="$PWD/server/conf/gateway-hunan.yaml $PWD/server/conf/gateway-beijing.yaml" \
./scripts/run_local_services.sh
```

## Smoke Commands

Focused protocol smoke tests:

```bash
./scripts/smoke_text_delivery.sh
./scripts/smoke_relationships.sh
```

They verify Gate login, Gateway authentication, TCP message delivery,
conversation persistence, ACK handling, relationship paths, and message
synchronization.

With the two-node service command above, run:

```bash
./scripts/smoke_gateway_message.sh
```

It verifies the `G x N` Gateway-Message stream topology, cross-Gateway text
delivery, idempotency, group conversation sequence allocation, and sync repair.

Manual smoke commands:

```bash
verify_response="$(curl -sS -H 'Content-Type: application/json' \
  -d '{"email":"local-smoke@example.com"}' \
  http://127.0.0.1:18080/post-verifycode)"
verification_code="$(python3 -c 'import json,sys; print(json.load(sys.stdin)["verificationCode"])' \
  <<<"$verify_response")"

curl -sS -H 'Content-Type: application/json' \
  -d "{\"username\":\"smoke_user\",\"password\":\"123456\",\"email\":\"local-smoke@example.com\",\"verifycode\":\"$verification_code\"}" \
  http://127.0.0.1:18080/post-signUp

curl -sS -H 'Content-Type: application/json' \
  -d '{"username":"zorjen","password":"123456"}' \
  http://127.0.0.1:18080/post-signIn
```
