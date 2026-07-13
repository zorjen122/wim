# wim

**Language:** English | [中文](README.zh-CN.md)

wim is a C++ instant messaging server playground with separate gate, verify,
state, chat, and file services. It was originally written as a local learning
project, and the repository now includes scripts and configuration conventions
for bringing the full flow back up on a development machine.

The main verified path is:

1. The client requests an email verification code through the gate service.
2. The gate forwards the request to the Node.js verify service.
3. The client registers and then signs in through the gate.
4. The gate asks the state service for an available chat node.
5. The client connects to the returned chat TCP endpoint.
6. Chat nodes deliver text messages locally or forward them to another chat node
   through gRPC when the receiver is online elsewhere.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `server/gate` | HTTP entrypoint for verification, registration, sign-in, and routing. |
| `server/verify` | Node.js gRPC verification-code service. |
| `server/state` | Chat-node selection service. |
| `server/chat` | TCP chat server, chat gRPC forwarding, friend/message data access. |
| `server/file` | File upload service. |
| `server/public` | Shared C++ utilities, protobuf definitions, database and Redis helpers. |
| `server/test` | Interactive and smoke-test client. |
| `server/conf` | Canonical local configuration directory. |
| `scripts` | Build, database initialization, Redis startup, service startup, and smoke tests. |
| `docs` | Requirements and feature-verification notes. |

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
- Node.js and npm for the verify service.
- `clang-format` for formatting checks.

## Configuration

Local service and test-client configs live in [server/conf](server/conf/README.md).
Scripts default to these files:

- `server/conf/gate.yaml`
- `server/conf/state-single.yaml`
- `server/conf/chat-hunan-im.yaml`
- `server/conf/test-client.yaml`
- `server/conf/verify.json`

`server/conf/verify.json` intentionally leaves private values blank. Supply
secrets through environment variables when real email or authenticated external
services are required:

```bash
export WIM_VERIFY_EMAIL_USER="your-email@example.com"
export WIM_VERIFY_EMAIL_PASS="your-email-app-password"
export WIM_VERIFY_REDIS_PASSWORD="root"
export WIM_VERIFY_MYSQL_PASSWORD="root"
```

For local smoke tests, `scripts/run_local_services.sh` starts verify with
`WIM_VERIFY_SEND_EMAIL=0`, so no real email is sent.

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

Start the default single-chat-node stack:

```bash
./scripts/run_local_services.sh
```

Start a two-chat-node stack for cross-node message forwarding:

```bash
WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
./scripts/run_local_services.sh
```

## Smoke Tests

After the services are running, run the single-node smoke test:

```bash
./scripts/smoke.sh
```

For a two-chat-node stack, run:

```bash
./scripts/smoke_multi_chat.sh
```

Additional focused checks:

```bash
./scripts/smoke_relationships.sh
./scripts/smoke_text_delivery.sh
```

The smoke scripts use the test client in `server/test` and cover registration,
sign-in routing, text messaging, friend/group relationship paths where
available, message pull behavior, and file upload.

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
- [docs/feature-verification.md](docs/feature-verification.md): feature list,
  verification status, code indexes, and manual test ideas.
- [CHANGELOG.md](CHANGELOG.md): verified baseline and subsequent architectural
  changes.
- [server/conf/README.md](server/conf/README.md): configuration naming and
  override rules.
