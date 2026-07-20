# WIM Qt 6 Android 构建与部署计划

> 基线：Ubuntu 26.04、Qt 6.10.2、首个 Android ABI 为 `arm64-v8a`
>
> 范围：先得到可安装、可启动、可连接本地 WIM 服务的调试 APK，再扩展多 ABI 和
> 发布 AAB。iOS 不与本阶段并行展开。

## 1. 当前状态

客户端的 CMake/QML 入口已经具备 Android 分支，Linux 专用 DBus 实现也由
`ANDROID` 条件隔离。当前主机已经安装 Qt 6.10.2 host/Android kit、OpenJDK 17、
Android SDK 36、Build Tools 36.0.0 和 NDK r27c，并实际生成 `arm64-v8a` Debug APK。

当前构建证据：

- CMake 识别目标 ABI 为 `arm64-v8a`，NDK Clang 18 完成全部 C++/QML AOT 编译；
- APK 包名 `org.wim.client`，版本 `0.1.0 (1)`，最低 API 28、目标 API 36；
- APK 只包含 `arm64-v8a`，并包含 `libQt6Protobuf_arm64-v8a.so`、SQLite driver、
  QML 插件和客户端主库；
- Debug APK 使用 Android 调试证书完成 APK Signature Scheme v2 签名验证；
- 已在 Android 14（API 34）arm64 真机完成安装、冷启动、重启和 Compact UI 验证；
- 已通过无线 ADB 和 `adb reverse` 到达本机 Auth Gate/Connection Gateway，真实登录
  与消息推送已进入问题修复后的复测阶段，完整断线补同步闭环仍待验收。

Protobuf 阻碍已经先行解除：

- 服务端继续以 Google Protobuf 生成和解析公开 `tcp_message.proto`。
- 客户端改用 `qt_add_protobuf`、`QProtobufSerializer` 和 `Qt6::Protobuf`。
- 客户端最终 ELF 依赖 `libQt6Protobuf.so.6`，不再依赖 Google
  `libprotobuf.so`。
- 黄金线格式测试由 Google `protoc` 生成固定字节，验证 QtProtobuf 的标量、
  `optional`、枚举和重复消息与服务端线格式一致。

这意味着 Android 构建只需要 Qt kit 自带的目标 ABI QtProtobuf；宿主机负责运行
`protoc`/`qtprotobufgen`，无需再单独交叉编译第三方 `libprotobuf`。

## 2. 固定工具链

Qt 6.10 官方 Android 基线使用 NDK r27c（`27.2.12479018`）和 JDK 17。首轮固定：

| 组件 | 版本/选择 | 原因 |
| --- | --- | --- |
| Qt host kit | Qt 6.10.2 `gcc_64` | 提供 host Qt 工具，与目标 kit 同版本 |
| Qt target kit | Qt 6.10.2 `android_arm64_v8a` | 第一目标是真机 arm64 |
| Qt 模块 | Base、Declarative、SVG、Protobuf | 对应当前客户端直接依赖 |
| JDK | OpenJDK 17 | 与 Qt 6.10 支持矩阵一致 |
| Android NDK | `27.2.12479018` | 与官方 Qt Android 二进制一致 |
| Android SDK | Platform 36、Build Tools 36、Platform Tools | 覆盖 Qt 6.10 支持的最新 API |
| 构建器 | Ninja | Qt Android 官方推荐路径 |

不在第一包中启用 `armeabi-v7a`、`x86` 或 `x86_64`。单 ABI 能把源码、QML、插件、
权限和网络问题与多 ABI 打包问题分开。真机闭环通过后，再为模拟器增加 `x86_64`，
发布 AAB 再决定是否保留 32 位 ABI。

## 3. 环境安装

先安装宿主工具：

```bash
sudo apt-get install -y openjdk-17-jdk ninja-build
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

使用 Qt Online Installer/Maintenance Tool 安装同一 Qt 6.10.2 版本的：

- Desktop `gcc_64`；
- Android `arm64-v8a`；
- 两个 kit 对应的 Qt GRPC/Protobuf 模块。

大陆网络可使用 Qt 在线安装器的清华 TUNA 镜像：

```bash
./qt-online-installer-linux-x64-online.run \
  --mirror https://mirrors.tuna.tsinghua.edu.cn/qt
```

推荐目录保持 Qt 默认布局：

```text
$HOME/Qt/6.10.2/gcc_64
$HOME/Qt/6.10.2/android_arm64_v8a
```

由 Qt Creator 的 Android 设置向导安装 SDK 最省步骤；手工安装时至少执行：

```bash
sdkmanager "platform-tools" \
  "platforms;android-36" \
  "build-tools;36.0.0" \
  "ndk;27.2.12479018"
```

环境验收：

```bash
java -version
adb version
$HOME/Android/Sdk/ndk/27.2.12479018/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --version
$HOME/Qt/6.10.2/android_arm64_v8a/bin/qt-cmake --version
```

## 4. 首个 arm64 调试 APK

从仓库根目录配置。移动端第一包关闭桌面测试目标；协议、repository 和 UI 自动化
仍由 Linux 构建执行。

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/27.2.12479018"

$HOME/Qt/6.10.2/android_arm64_v8a/bin/qt-cmake \
  -S client \
  -B build/client-android-arm64 \
  -GNinja \
  -DQT_HOST_PATH="$HOME/Qt/6.10.2/gcc_64" \
  -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
  -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF

cmake --build build/client-android-arm64 --target wim-client_make_apk -j2
```

项目 Android 包模板已把 Gradle 8.14.3 发行包切换到华为云镜像，并优先通过阿里云
Maven 代理解析 Google/AndroidX/Gradle Plugin 依赖，避免大陆环境在默认海外地址
静默超时。

构建必须满足：

- CMake 日志显示目标 ABI 为 `arm64-v8a`；
- `qtprotobufgen` 在宿主机运行；
- 生成的目标链接 Qt Android 的 `Qt6::Protobuf`；
- `apk` 目标成功，`android-build/build/outputs/apk` 下产生调试 APK；
- APK 内同时包含客户端主库、`wim-client-protocol` 和所需 Qt/QML 插件。

## 5. 安装与启动验证

开启真机开发者模式和 USB 调试后：

```bash
adb devices
find build/client-android-arm64/android-build/build/outputs/apk \
  -name '*.apk' -print
adb install -r <上一步输出的-debug.apk>
```

首包只验收平台启动闭环：

1. 应用可以安装和启动，不在 native loader/QML import 阶段退出。
2. Compact shell、底部导航、浅色/深色主题正常。
3. SQLite 可以创建并在重启后恢复。
4. 旋转、系统返回和软键盘不会导致页面卡死或内容永久遮挡。
5. `adb logcat` 中没有缺失 `Qt6Protobuf`、QML 模块或 SQL driver 的错误。

## 6. 连接本地服务端

Android 设备中的 `127.0.0.1` 指向设备自身，不能使用桌面测试命令中的
`http://127.0.0.1:18080`。

- 真机：使用开发机的局域网地址，例如 `http://192.168.1.20:18080`。
- Android Emulator：宿主机通常通过 `10.0.2.2` 访问。

开发期还有一个仅在 ADB 连接存续期间有效的例外：可以反向映射本机服务端口，此时
设备上的 `127.0.0.1` 会经 ADB 到达开发机：

```bash
adb reverse tcp:18080 tcp:18080
adb reverse tcp:8090 tcp:8090
adb reverse tcp:8091 tcp:8091
```

无线调试重连后必须重新建立映射；这不是发布环境的网络方案。

Gate 和 Connection Gateway 已监听 IPv4 任意地址，但 State 返回给客户端的 Gateway
地址来自 `state-*.yaml`。因此仅修改 `--gate-url` 不够：Android 联调配置还必须把
`hunan-gateway`/`beijing-gateway` 的 advertised host 从 `127.0.0.1` 改为设备可达的
开发机地址，同时开放 TCP `18080`、`8090` 和 `8091`。

移动端已经提供“设置 → 服务端”入口，并在网络登录页保留同样的纠错入口。Auth Gate
地址持久化到 `QSettings`，不写死个人局域网 IP，保存后完全退出并重启客户端生效；
清空地址后重启会返回本地预览模式。正式发布前再把开发地址替换为 HTTPS 域名和证书
策略。

## 7. Android 功能适配顺序

### A0：可构建、可安装

- [x] 完成工具链和 arm64 APK。
- [x] 固定应用显示名、版本号和包名 `org.wim.client`。
- [x] 静态验证 QML、SQLite、QtProtobuf 随 APK 部署。
- [x] 连接 arm64 真机，验证安装、冷启动和重启。

退出门槛：卸载后全新安装、冷启动、后台切回和重启均无崩溃。

### A1：移动 UI 与生命周期

- 系统返回优先关闭对话框/会话页，再退出应用。
- 验证 safe area、屏幕旋转和软键盘避让。
- 应用进入后台时允许 TCP 断开；回到前台后重连并按游标同步。

退出门槛：不依赖后台常驻连接也能恢复到一致会话状态。

### A2：真机网络闭环

- [x] 增加开发服务器地址设置和登录页纠错入口。
- 真机登录两个测试账号并完成双向文本、离线补同步和重启恢复。
- 记录 Gate 返回的 Gateway 地址，证明设备可访问被选择的 `8090/8091`。

退出门槛：消息恰好显示一次，客户端重启和 Gateway 断流后可恢复。

### A3：平台服务

- Android Keystore 保存正式 refresh/token 凭据。
- 通知渠道、本地通知和系统分享。
- 服务端具备移动推送契约后再接 FCM；FCM 只负责提醒/唤醒，消息完整性仍由同步保证。

### A4：发布包

- 增加 `x86_64` 或其他明确需要的 ABI。
- 使用 `aab` 目标生成 Android App Bundle。
- Keystore、别名和密码只通过本机/CI secret 注入，不写入仓库。
- 在 Play 内部测试或等价封闭渠道验证升级安装和数据库迁移。

## 8. 完成判据

Android 第一阶段只有同时具备以下证据才算完成：

- arm64 Debug APK 的实际构建产物；
- 真机或模拟器安装和冷启动日志；
- QtProtobuf/SQLite/QML 插件均由 APK 正确加载；
- 真机登录、双向文本、断线恢复和本地重启恢复记录；
- Linux 客户端全量测试继续通过；
- Release AAB 能构建并完成签名配置检查。

当前已经完成 Protobuf 前置项、Android 工具链、真机安装/冷启动、Compact UI 和
运行时 Gate 地址配置；下一执行点是安装最新修复包，完成双向文本、Gateway 断流、
离线补同步和本地重启恢复的完整真机验收。
