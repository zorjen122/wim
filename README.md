# wim

**Language:** English | [中文](README.zh-CN.md)

wim is a C++20 instant messaging system with Auth/API Gate, Connection Gateway,
State, Message, and File nodes. The repository includes scripts and
configuration conventions for bringing the full flow up on a development
machine.

The main verified path is:

1. The client requests an email verification code through the gate service.
2. Gate generates, rate-limits, stores, and verifies the code through Redis; it
   can deliver real mail through libcurl SMTP.
3. The client registers and signs in through Gate.
4. Gate asks State for a Connection Gateway endpoint and returns a short-lived
   connection token.
5. The client keeps a TCP connection to Connection Gateway.
6. Gateway and Message nodes exchange commands and deliveries through their
   existing bidirectional gRPC streams.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `server/gate` | HTTP entrypoint for verification, registration, sign-in, and routing. |
| `server/gateway` | Long-lived client connections, authentication, flow control, and physical push. |
| `server/state` | Gateway placement and versioned Message-node topology. |
| `server/chat` | Message node: relationships, conversations, persistence, and delivery generation. |
| `server/file` | File upload service. |
| `server/public` | Shared C++ utilities, protobuf definitions, database and Redis helpers. |
| `server/conf` | Canonical local configuration directory. |
| `scripts` | Build, database initialization, Redis startup, service startup, and smoke tests. |
| `docs` | Public requirements and feature-verification status. |

## Requirements

See [docs/requirements.md](docs/requirements.md) for the full dependency list
and package installation commands.

At a high level, the local environment needs:

- C++ build tools: `clang`, `cmake`, `build-essential`, `pkg-config`.
- gRPC and protobuf development packages, plus Python protobuf support for the
  protocol-level smoke scripts.
- Boost, yaml-cpp, spdlog, fmt, jsoncpp, hiredis, and librdkafka development
  packages.
- MySQL Server with MySQL X Plugin enabled.
- MySQL Connector/C++ with X DevAPI support.
- Redis.
- libcurl development files for SMTP delivery from Gate.
- `clang-format` for formatting checks.

## Configuration

Local service and test-client configs live in [server/conf](server/conf/README.md).
Scripts default to these files:

- `server/conf/gate.yaml`
- `server/conf/state-single.yaml`
- `server/conf/chat-hunan-im.yaml`
- `server/conf/gateway-hunan.yaml`
- `server/conf/test-client.yaml`

Local `gate.yaml` disables real mail and exposes the code in the HTTP response.
For SMTP delivery, disable `exposeCodeInResponse`, enable the email section, and
supply secrets through environment variables:

```bash
export WIM_VERIFY_EMAIL_USER="your-email@example.com"
export WIM_VERIFY_EMAIL_PASS="your-email-app-password"
export WIM_VERIFY_SEND_EMAIL=1
export WIM_VERIFY_EXPOSE_CODE=0
```

## Build

```bash
./scripts/build.sh
```

The build output is written to `build/wim`, and generated protobuf/gRPC C++
sources are produced inside the CMake build tree.

## Initialize Local Data

```bash
./scripts/init_mysql.sh
./scripts/start_redis.sh
```

The default local Redis endpoint is `127.0.0.1:6380` with password `root`.
MySQL defaults are documented in [docs/requirements.md](docs/requirements.md).

## Run Services

Start the default local Gateway/Message stack:

```bash
./scripts/run_local_services.sh
```

Start a `G=2, N=2` local topology:

```bash
WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
WIM_GATEWAY_CONFIGS="$PWD/server/conf/gateway-hunan.yaml $PWD/server/conf/gateway-beijing.yaml" \
./scripts/run_local_services.sh
```

## Smoke Tests

After the default services are running, run the focused protocol smoke tests:

```bash
./scripts/smoke_text_delivery.sh
./scripts/smoke_relationships.sh
```

For the `G=2, N=2` topology, run:

```bash
./scripts/smoke_gateway_message.sh
```

These smoke scripts use the Python TCP protocol helper in `scripts/lib` and
cover Gate login, Gateway authentication, text delivery, relationship paths,
message synchronization, ACK handling, and Gateway-Message routing.

## Formatting

The repository includes a root `.clang-format`. Check current C/C++ changes with:

```bash
./scripts/check_format.sh
```

Check all tracked C/C++ files:

```bash
./scripts/check_format.sh --all
```

The script prefers `clang-format --dry-run --Werror` and falls back to a diff
check when running against older clang-format versions.

## More Documentation

- [docs/requirements.md](docs/requirements.md): dependency, build, local port,
  and smoke-test details.
- [docs/feature-server-verification.md](docs/feature-server-verification.md):
  server feature list, verification status, and code indexes.
- [docs/client-feature-verification.md](docs/client-feature-verification.md):
  client feature list, verification status, and code indexes.
- [CHANGELOG.md](CHANGELOG.md): verified baseline and subsequent architectural
  changes.
- [server/conf/README.md](server/conf/README.md): configuration naming and
  override rules.
