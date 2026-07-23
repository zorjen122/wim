# wimi

**语言：** [English](README.md) | 中文

wimi 是一个 C++20 即时通讯系统，按职责拆分为 Auth/API Gate、Connection
Gateway、State、Message 和 File 节点。仓库提供本地构建、集中配置、数据库
初始化、Redis 启动和 smoke 测试脚本。

主要流程如下：

1. 客户端通过 gate 请求邮箱验证码。
2. Gate 在进程内生成验证码，并通过 Redis 完成限频、保存和消费；真实邮件由
   libcurl SMTP 投递。
3. 客户端通过 Gate 注册和登录。
4. Gate 向 State 请求 Connection Gateway 地址并签发短期连接 token。
5. 客户端与 Connection Gateway 保持 TCP 长连接。
6. Gateway 与 Message 节点通过既有双向 gRPC 流传递命令和推送。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `server/gate` | 验证码、注册、登录和节点路由的 HTTP 入口。 |
| `server/gateway` | 客户端长连接、鉴权、流控和物理推送。 |
| `server/state` | Gateway 选址和版本化 Message 节点拓扑。 |
| `server/message` | Message 节点：关系、会话、消息持久化和投递生成。 |
| `server/file` | 文件上传服务。 |
| `server/public` | 公共 C++ 工具、protobuf、数据库和 Redis 封装。 |
| `server/conf` | 本地配置文件统一目录。 |
| `scripts` | 构建、初始化、启动和 smoke 脚本。 |
| `docs` | 对外依赖说明和配置文档。 |

## 依赖

完整依赖和安装命令见 [docs/requirements.md](docs/requirements.md)。

本地环境大致需要：

- C++ 构建工具：`clang`、`cmake`、`build-essential`、`pkg-config`。
- gRPC 和 protobuf 开发包，以及协议级 smoke 脚本所需的 Python protobuf 支持。
- Boost、yaml-cpp、spdlog、fmt、jsoncpp、hiredis、librdkafka 开发包。
- 启用 MySQL X Plugin 的 MySQL Server。
- 支持 X DevAPI 的 MySQL Connector/C++。
- Redis。
- Gate SMTP 投递所需的 libcurl 开发包。
- 格式检查所需的 `clang-format`。

## 配置

本地服务和测试客户端配置统一放在 [server/conf](server/conf/README.md)。
脚本默认使用：

- `server/conf/gate.yaml`
- `server/conf/state-single.yaml`
- `server/conf/message-hunan-im.yaml`
- `server/conf/gateway-hunan.yaml`
- `server/conf/test-client.yaml`

本地 `gate.yaml` 默认不发送真实邮件，而是在 HTTP 响应中返回验证码。启用 SMTP
时应关闭 `exposeCodeInResponse`、启用 email 配置，并用环境变量注入密钥：

```bash
export WIMI_VERIFY_EMAIL_USER="your-email@example.com"
export WIMI_VERIFY_EMAIL_PASS="your-email-app-password"
export WIMI_VERIFY_SEND_EMAIL=1
export WIMI_VERIFY_EXPOSE_CODE=0
```

## 构建

```bash
./scripts/build.sh
```

构建产物会写入 `build/wimi`，protobuf/gRPC 生成文件位于 CMake 构建目录中。

## 初始化数据

```bash
./scripts/init_mysql.sh
./scripts/start_redis.sh
```

默认本地 Redis 地址为 `127.0.0.1:6380`，密码为 `root`。MySQL 默认设置见
[docs/requirements.md](docs/requirements.md)。

## 启动服务

启动默认本地 Gateway/Message 服务：

```bash
./scripts/run_local_services.sh
```

启动 `G=2、N=2` 本地拓扑：

```bash
WIMI_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
WIMI_MESSAGE_CONFIGS="$PWD/server/conf/message-hunan-im.yaml $PWD/server/conf/message-beijing-im.yaml" \
WIMI_GATEWAY_CONFIGS="$PWD/server/conf/gateway-hunan.yaml $PWD/server/conf/gateway-beijing.yaml" \
./scripts/run_local_services.sh
```

## Smoke 测试

默认服务启动后，运行聚焦协议 smoke：

```bash
./scripts/smoke_text_delivery.sh
./scripts/smoke_relationships.sh
```

`G=2、N=2` 拓扑下运行：

```bash
./scripts/smoke_gateway_message.sh
```

这些 smoke 脚本使用 `scripts/lib` 中的 Python TCP 协议 helper，覆盖 Gate
登录、Gateway 鉴权、文本投递、关系路径、消息同步、ACK 处理和 Gateway-Message
路由。

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
- [server/conf/README.md](server/conf/README.md)：配置命名和覆盖规则。
