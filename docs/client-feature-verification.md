# Wim Client 功能清单与验证状态

说明：本文档基于当前本地代码树和已经跑通的 smoke、CTest、构建流程整理。`已验证` 表示已经通过脚本、CTest、构建或端到端 smoke 断言；`未验证` 表示代码入口存在但本轮未做端到端断言，或当前实现仍是骨架、未注册、未完整接通。

## Client 功能清单

| 编号 | 功能 | 状态 | 主要索引 |
| --- | --- | --- | --- |
| CV1 | Qt 6/QML/C++20 客户端本地构建 | 已验证 | [client CMake](../client/CMakeLists.txt#L1)、[Linux 构建说明](../client/README.md#L22) |
| CV2 | QtProtobuf 绑定公开 `tcp_message.proto` | 已验证 | [qt_add_protobuf](../client/CMakeLists.txt#L20)、[协议 codec](../client/src/adapters/connection_gateway/ProtobufPacketCodec.cc#L1) |
| CV3 | Fake Scenario、领域模型和列表模型 | 已验证 | [Fake Repository](../client/src/adapters/fake/FakeScenarioRepository.cc#L1)、[model 测试](../client/tests/unit/ClientModelsTest.cc#L98) |
| CV4 | 响应式 QML Shell 和第一阶段 UI 页面 | 已验证 | [AdaptiveShell](../client/qml/AdaptiveShell.qml#L1)、[QML module](../client/CMakeLists.txt#L81) |
| CV5 | Auth Gate HTTP adapter：登录、验证码、注册、忘记密码 | 已验证 | [GateHttpClient](../client/src/adapters/gate/GateHttpClient.cc#L1)、[network 测试](../client/tests/unit/ClientNetworkTest.cc#L137) |
| CV6 | Connection Gateway TCP/帧协议/请求队列/ACK | 已验证 | [ConnectionGatewayClient](../client/src/adapters/connection_gateway/ConnectionGatewayClient.cc#L1)、[network 测试](../client/tests/unit/ClientNetworkTest.cc#L265) |
| CV7 | SQLite repository v3、outbox、草稿、同步游标和幂等入站 | 已验证 | [SQLite repository](../client/src/adapters/sqlite/SqliteClientRepository.cc#L52)、[SQLite 单元测试](../client/tests/unit/ClientModelsTest.cc#L426) |
| CV8 | SQLite 空库表结构初始化脚本 | 已验证 | [初始化脚本](../client/sql/init_client_sqlite.sql#L1) |
| CV9 | 真实服务核心单聊链路 live integration | 已验证 | [live test](../client/tests/integration/ClientLiveNetworkTest.cc#L21)、[真实服务测试说明](../client/README.md#L132) |
| CV10 | QML 静态检查和离屏 UI smoke | 已验证 | [qmllint/smoke CTest](../client/CMakeLists.txt#L285)、[main smoke 参数](../client/src/main.cc#L43) |
| CV11 | Linux 桌面通知端口与安装布局 | 已验证 | [LinuxDesktopServices](../client/src/platform/linux/LinuxDesktopServices.cc#L1)、[Linux 桌面集成说明](../client/README.md#L155) |
| CV12 | Android arm64 Debug APK 构建基线 | 已验证 | [Android package 设置](../client/CMakeLists.txt#L148)、[Android 状态说明](../client/README.md#L174) |
| CU1 | Windows 客户端构建与平台服务 | 未验证 | [平台服务工厂](../client/src/platform/PlatformServicesFactory.cc#L1) |
| CU2 | iOS 客户端构建、签名与平台服务 | 未验证 | [AdaptiveShell](../client/qml/AdaptiveShell.qml#L1) |
| CU3 | 附件/文件发送 UI 与本地文件缓存 | 未验证 | [文件上传请求接口](../client/src/adapters/connection_gateway/ConnectionGatewayClient.cc#L194) |
| CU4 | 历史消息反向分页/加载更早消息 | 未验证 | [PullConversationMessages](../client/src/adapters/connection_gateway/ConnectionGatewayClient.cc#L143) |
| CU5 | 资料编辑、完整群成员、退群、删好友、服务端搜索 | 未验证 | [SettingsPage](../client/qml/SettingsPage.qml#L1)、[协议边界](../client/README.md#L112) |
| CU6 | outbox 自动退避重试策略 | 未验证 | [outbox 字段说明](../client/sql/init_client_sqlite.sql#L64) |
| CU7 | 凭证安全存储、TLS 与公网安全基线 | 未验证 | [客户端 README 安全说明](../client/README.md#L116) |
| CU8 | Android/iOS 原生通知、文件选择和分享面板 | 未验证 | [平台服务端口](../client/src/ports/IPlatformServices.h#L1) |

## Client 已验证功能

| 编号 | 预期行为 | 代码索引 | 测试方法 |
| --- | --- | --- | --- |
| CV1 | 在 Linux Debug preset 或根目录 CMake 命令下完成 Qt 6 客户端配置、编译和 CTest。 | [client CMake](../client/CMakeLists.txt#L1)、[Linux 构建说明](../client/README.md#L22) | 执行 `cmake --preset linux-debug`、`cmake --build --preset linux-debug`、`ctest --preset linux-debug`；当前客户端阶段一基线已跑通。 |
| CV2 | 客户端只从公开 `server/public/proto/tcp_message.proto` 生成 QtProtobuf 类型，不依赖内部 Gateway-Message 协议或 Google `libprotobuf`。 | [qt_add_protobuf](../client/CMakeLists.txt#L20)、[ProtobufPacketCodec](../client/src/adapters/connection_gateway/ProtobufPacketCodec.cc#L1)、[协议边界说明](../client/README.md#L102) | `client-network` 中 `qtProtobufMatchesCanonicalWireFormat` 和 `optionalPacketTimestampDefaultsToEmpty` 覆盖序列化、反序列化和可选字段。 |
| CV3 | Fake Repository 暴露 13 个确定性 UI 场景；会话、消息、联系人、申请 model 正确暴露 role 并支持状态更新。 | [Fake Repository](../client/src/adapters/fake/FakeScenarioRepository.cc#L1)、[ClientModelsTest](../client/tests/unit/ClientModelsTest.cc#L98) | `client-models` 覆盖 scenario 列表、large-history、会话 role、未读、草稿、发送生命周期、联系人和申请更新。 |
| CV4 | UI 支持 Expanded/Medium/Compact 布局，包含会话、联系人、申请、设置、登录覆盖层和紧凑模式返回。 | [AdaptiveShell](../client/qml/AdaptiveShell.qml#L1)、[Main.qml](../client/qml/Main.qml#L1)、[QML smoke](../client/CMakeLists.txt#L285) | `ui-smoke-*` 覆盖 compact、medium、expanded、深色、缩放、会话页、联系人、申请、设置、认证覆盖层和 large-history。 |
| CV5 | Auth Gate adapter 能构造验证码、注册、登录、忘记密码 HTTP JSON 请求，并从登录响应解析 Gateway session。 | [GateHttpClient](../client/src/adapters/gate/GateHttpClient.cc#L25)、[ClientNetworkTest](../client/tests/unit/ClientNetworkTest.cc#L137) | `client-network` 使用本地 `QTcpServer` 模拟 Gate，断言请求路径、请求体和 `GateSession` 字段。 |
| CV6 | Connection Gateway adapter 能完成 TCP Gateway 登录、支持公开请求、按响应类型排队、发送 TRANSPORT/DELIVERED/READ ACK，并能重连再认证。 | [ConnectionGatewayClient](../client/src/adapters/connection_gateway/ConnectionGatewayClient.cc#L27)、[ClientNetworkTest](../client/tests/unit/ClientNetworkTest.cc#L265) | `client-network` 使用本地 TCP server 覆盖 supported request/receipt contracts 和 reconnect/authenticate again。 |
| CV7 | SQLite repository 能迁移到 v3，持久化 outbox、草稿、sync cursor、远端会话映射，并幂等写入大批量入站消息。 | [SqliteClientRepository](../client/src/adapters/sqlite/SqliteClientRepository.cc#L52)、[SQLite 测试](../client/tests/unit/ClientModelsTest.cc#L426) | `client-models` 覆盖 `sqlitePersistsOutboxDraftAndSyncCursor`、迁移、拒绝新版本、幂等 batch、network projection。 |
| CV8 | 客户端提供空库初始化 SQL，表结构、索引和 `user_version=3` 与当前 repository schema 对齐。 | [init_client_sqlite.sql](../client/sql/init_client_sqlite.sql#L1)、[README 入口](../client/README.md#L88) | 执行 `sqlite3 /tmp/wim-client-schema-check.sqlite < client/sql/init_client_sqlite.sql`；本轮已验证 SQL 可执行。 |
| CV9 | 在本地真实服务已启动且设置 live env 后，客户端 adapter 可以完成 Gate 登录、Gateway 鉴权、好友拉取、单聊发送、接收推送、ACK 和增量同步。 | [ClientLiveNetworkTest](../client/tests/integration/ClientLiveNetworkTest.cc#L21)、[README live test](../client/README.md#L132) | 设置 `WIM_ENABLE_LIVE_TESTS=ON` 构建后，运行 `ctest --test-dir build/client-clang -R '^client-live-network$' --output-on-failure`。 |
| CV10 | QML 静态检查无警告；离屏 smoke 不只启动窗口，还断言关键列表非空。 | [CTest smoke](../client/CMakeLists.txt#L285)、[main assert-populated](../client/src/main.cc#L43) | 执行 `cmake --build build/client-clang --target wim-client_qmllint` 和 `ctest --test-dir build/client-clang`。 |
| CV11 | Linux 下通过 freedesktop Notifications adapter 暴露通知可用性并支持设置页测试通知；安装布局包含 desktop 文件和 SVG 图标。 | [LinuxDesktopServices](../client/src/platform/linux/LinuxDesktopServices.cc#L24)、[安装说明](../client/README.md#L155) | `client-models` 覆盖平台通知端口；安装布局可用 `cmake --install build/client-clang --prefix /tmp/wim-client-install` 检查。 |
| CV12 | Android arm64 Debug APK 可以构建，包名、QtProtobuf、SQLite 和 QML 插件随包部署。 | [Android CMake 属性](../client/CMakeLists.txt#L148)、[Android 状态](../client/README.md#L174) | 当前基线已使用 Qt 6.10.2、NDK r27c、SDK 36 构建 `android-build-debug.apk`。 |

## Client 未验证或待实现功能

| 编号 | 预期行为 | 代码索引 | 测试方法 |
| --- | --- | --- | --- |
| CU1 | Windows 平台应能完成构建、启动和平台服务适配。当前未建立 Windows kit、CI 或真机/虚拟机验证。 | [平台服务工厂](../client/src/platform/PlatformServicesFactory.cc#L1) | 使用 Windows Qt 6 kit 运行 CMake/CTest/QML smoke，并验证 notification 或默认 platform service。 |
| CU2 | iOS 平台应能完成构建、签名、真机启动和安全区/键盘/平台服务验证。当前仅做架构预留。 | [AdaptiveShell](../client/qml/AdaptiveShell.qml#L1) | 使用 Xcode、Qt iOS kit 和 Apple 签名构建真机包，并验证登录页、紧凑布局、网络和安全区。 |
| CU3 | 附件发送应具备 UI、对象存储/文件缓存、本地 metadata 和服务端完整链路。当前只有 C++ `UploadFile` 请求接口，UI 不伪造附件发送。 | [UploadFile 接口](../client/src/adapters/connection_gateway/ConnectionGatewayClient.cc#L194)、[当前边界](../client/README.md#L112) | 需要服务端文件消息契约稳定，再验证附件表、选择器、上传进度、失败重试、fake、unit 和 live test。 |
| CU4 | 消息时间线应支持加载更早历史消息。当前公开协议只有 `after_seq` 增量方向，客户端不伪造反向分页。 | [PullConversationMessages](../client/src/adapters/connection_gateway/ConnectionGatewayClient.cc#L143) | 需要服务端提供 `before_seq` 或分页锚点，再验证 repository 分页游标和 UI 上拉加载。 |
| CU5 | 资料编辑、完整群成员、退群、删好友、服务端搜索应在 handler 稳定后开放。当前 UI 只保留诚实占位或不提供入口。 | [SettingsPage 资料占位](../client/qml/SettingsPage.qml#L1)、[协议边界](../client/README.md#L112) | 需要服务端注册对应 handler，再验证 adapter 方法、repository 更新、UI 状态和 live integration。 |
| CU6 | outbox 应支持自动退避重试、attempt count 和 next retry 时间。当前字段与索引已预留，恢复逻辑主要按消息状态扫描。 | [outbox schema](../client/sql/init_client_sqlite.sql#L64)、[ResumePendingOutgoing](../client/src/app/AppController.cc#L1271) | 增加 retry scheduler 单元测试，断言网络失败、重启恢复、退避窗口和最终 ACCEPTED/失败状态。 |
| CU7 | 公网或长期登录前，应增加 TLS、token 生命周期管理和系统安全存储。当前只保存移动端 Gate URL，不保存长期凭据。 | [README 安全说明](../client/README.md#L116)、[PlatformServices](../client/src/ports/IPlatformServices.h#L1) | 需要明确服务端 token/refresh 契约，再验证 Secret Service/Android Keystore/iOS Keychain/Windows Credential Manager。 |
| CU8 | Android/iOS 原生通知、文件选择和分享面板应通过平台服务端口实现。当前只有 Linux freedesktop Notifications adapter。 | [IPlatformServices](../client/src/ports/IPlatformServices.h#L1)、[Linux 实现](../client/src/platform/linux/LinuxDesktopServices.cc#L1) | 在 Android/iOS 平台实现 permission、notification、file picker、share sheet，并以真机 smoke 和 mock platform service 覆盖。 |
