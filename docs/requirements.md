# wim Local Build Requirements

## System Packages

Tested on Ubuntu 26.04 with clang.

| Component | Used by | Packages / runtime |
| --- | --- | --- |
| C++ toolchain | all C++ services | `clang`, `cmake`, `build-essential`, `pkg-config` |
| Protobuf / gRPC | gate, state, chat, file, verify RPC clients, protocol smoke scripts | `protobuf-compiler`, `protobuf-compiler-grpc`, `libprotobuf-dev`, `libgrpc++-dev`, `libgrpc-dev`, `python3-protobuf` |
| Boost.Asio / Beast | gate, chat, test client | `libboost-dev`, `libboost-filesystem-dev` |
| Config / logging / JSON | all C++ services | `libyaml-cpp-dev`, `libspdlog-dev`, `libfmt-dev`, `libjsoncpp-dev` |
| Redis | verify codes, online users, ids, de-dup cache | `redis-server`, `libhiredis-dev` |
| MySQL | users, user info, friends, messages | `mysql-server`, MySQL Connector/C++ X DevAPI packages |
| Kafka placeholder | chat utility code links librdkafka | `librdkafka-dev` |
| Node verify service | verify gRPC service | `nodejs`, `npm`, `server/verify/package-lock.json` |

Install from Ubuntu repositories:

```bash
sudo apt-get install -y \
  clang cmake build-essential pkg-config \
  protobuf-compiler protobuf-compiler-grpc libprotobuf-dev python3-protobuf \
  libgrpc++-dev libgrpc-dev \
  libboost-dev libboost-filesystem-dev \
  libyaml-cpp-dev libspdlog-dev libfmt-dev libjsoncpp-dev \
  libhiredis-dev redis-server mysql-server \
  librdkafka-dev nodejs npm
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
- `50051`: verify gRPC
- `50052`: state gRPC
- `50055`: chat gRPC
- `50056`: second chat gRPC in two-node mode
- `8090`: chat TCP
- `8091`: second chat TCP in two-node mode
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

For local smoke tests, verify runs with `WIM_VERIFY_SEND_EMAIL=0`, so it writes the verification code to Redis without sending real mail.

To run two chat nodes for cross-node forwarding:

```bash
WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
./scripts/run_local_services.sh
```

## Smoke Commands

Automated smoke test:

```bash
./scripts/smoke.sh
```

It verifies `gate -> verify`, registration, `gate -> state` login routing,
chat TCP offline persistence, online same-node delivery, and `chat -> file`
upload.

With the two-node service command above, run:

```bash
./scripts/smoke_multi_chat.sh
```

It verifies online text forwarding from chat node `8090` to chat node `8091`
through the chat gRPC link.

Manual smoke commands:

```bash
curl -sS -H 'Content-Type: application/json' \
  -d '{"email":"local-smoke@example.com"}' \
  http://127.0.0.1:18080/post-verifycode

curl -sS -H 'Content-Type: application/json' \
  -d '{"username":"smoke_user","password":"123456","email":"smoke@example.com"}' \
  http://127.0.0.1:18080/post-signUp

curl -sS -H 'Content-Type: application/json' \
  -d '{"username":"zorjen","password":"123456"}' \
  http://127.0.0.1:18080/post-signIn

printf 'textSend\n1002\nhello_from_smoke\nq\n' | \
  WIM_CONFIG="$PWD/server/conf/test-client.yaml" \
  ./build/wim/test/imTest zorjen 123456 1001 127.0.0.1 8090

printf 'file smoke\n' > server/test/wim_upload_smoke.txt
printf 'uploadFile\nwim_upload_smoke.txt\nq\n' | \
  (cd server/test && \
    WIM_CONFIG="$PWD/../conf/test-client.yaml" \
    ../../build/wim/test/imTest zorjen 123456 1001 127.0.0.1 8090)
```
