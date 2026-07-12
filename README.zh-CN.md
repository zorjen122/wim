# wim

**语言：** [English](README.md) | 中文

wim 是一个 C++ 即时通讯服务器练习项目，按职责拆分为 gate、verify、state、
chat、file 等服务。当前仓库已经补充了本地构建、配置集中管理、数据库初始化、
Redis 启动和 smoke 测试脚本，便于重新把完整流程跑起来。

主要流程如下：

1. 客户端通过 gate 请求邮箱验证码。
2. gate 将验证码请求转发到 Node.js verify 服务。
3. 客户端提交注册信息，然后通过 gate 登录。
4. gate 向 state 请求可用 chat 节点。
5. 客户端拿到 chat TCP 端点后建立连接。
6. chat 节点本地投递在线消息；如果接收者在其他 chat 节点，则通过 chat gRPC
   转发。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `server/gate` | 验证码、注册、登录和节点路由的 HTTP 入口。 |
| `server/verify` | Node.js gRPC 验证码服务。 |
| `server/state` | chat 节点选择服务。 |
| `server/chat` | TCP 聊天服务、chat gRPC 转发、好友和消息数据访问。 |
| `server/file` | 文件上传服务。 |
| `server/public` | 公共 C++ 工具、protobuf、数据库和 Redis 封装。 |
| `server/test` | 交互式测试客户端和 smoke 测试入口。 |
| `server/conf` | 本地配置文件统一目录。 |
| `scripts` | 构建、初始化、启动和 smoke 脚本。 |
| `docs` | 依赖说明和功能验证文档。 |

## 依赖

完整依赖和安装命令见 [docs/requirements.md](docs/requirements.md)。

本地环境大致需要：

- C++ 构建工具：`clang`、`cmake`、`build-essential`、`pkg-config`。
- gRPC 和 protobuf 开发包，以及协议级 smoke 脚本所需的 Python protobuf 支持。
- Boost、yaml-cpp、spdlog、fmt、jsoncpp、hiredis、librdkafka 开发包。
- 启用 MySQL X Plugin 的 MySQL Server。
- 支持 X DevAPI 的 MySQL Connector/C++。
- Redis。
- verify 服务所需的 Node.js 和 npm。
- 格式检查所需的 `clang-format`。

## 配置

本地服务和测试客户端配置统一放在 [server/conf](server/conf/README.md)。
脚本默认使用：

- `server/conf/gate.yaml`
- `server/conf/state-single.yaml`
- `server/conf/chat-hunan-im.yaml`
- `server/conf/test-client.yaml`
- `server/conf/verify.json`

`server/conf/verify.json` 中的隐私字段刻意留空。需要真实发邮件或连接带认证的
外部服务时，通过环境变量注入：

```bash
export WIM_VERIFY_EMAIL_USER="your-email@example.com"
export WIM_VERIFY_EMAIL_PASS="your-email-app-password"
export WIM_VERIFY_REDIS_PASSWORD="root"
export WIM_VERIFY_MYSQL_PASSWORD="root"
```

本地 smoke 测试中，`scripts/run_local_services.sh` 会使用
`WIM_VERIFY_SEND_EMAIL=0` 启动 verify，因此不会发送真实邮件。

## 构建

```bash
./scripts/build.sh
```

构建产物会写入 `build/wim`，protobuf/gRPC 生成文件位于 CMake 构建目录中。

## 初始化数据

```bash
./scripts/init_mysql.sh
./scripts/start_redis.sh
```

默认本地 Redis 地址为 `127.0.0.1:6380`，密码为 `root`。MySQL 默认设置见
[docs/requirements.md](docs/requirements.md)。

## 启动服务

启动默认单 chat 节点服务：

```bash
./scripts/run_local_services.sh
```

启动双 chat 节点，用于验证跨节点消息转发：

```bash
WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
./scripts/run_local_services.sh
```

## Smoke 测试

服务启动后，运行单节点 smoke：

```bash
./scripts/smoke.sh
```

双 chat 节点模式下运行：

```bash
./scripts/smoke_multi_chat.sh
```

其他聚焦测试：

```bash
./scripts/smoke_relationships.sh
./scripts/smoke_text_delivery.sh
```

这些脚本会使用 `server/test` 中的测试客户端，覆盖注册、登录路由、文本消息、
好友/群关系相关路径、消息拉取和文件上传等能力。

## 格式化

仓库根目录提供了 `.clang-format`。检查当前 C/C++ 变更：

```bash
./scripts/check_format.sh
```

检查所有已纳管 C/C++ 文件：

```bash
./scripts/check_format.sh --all
```

脚本会优先使用 `clang-format --dry-run --Werror`，在旧版本 clang-format 上回退
到 diff 检查。

## 更多文档

- [docs/requirements.md](docs/requirements.md)：依赖、构建、本地端口和 smoke
  流程。
- [docs/feature-verification.md](docs/feature-verification.md)：功能清单、验证状态、
  代码索引和手动测试建议。
- [server/conf/README.md](server/conf/README.md)：配置命名和覆盖规则。
