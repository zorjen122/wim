# WIM Qt 6 + QML 客户端规划

> 状态：执行基线 v1.6（UI-0～UI-3、DATA-1～DATA-2、Linux 通知、网络阶段一已落地；核心单聊真实联调已验证）
> 日期：2026-07-18
> 目标平台：Windows、iOS、Android、Linux；不包含 Web
> 首要平台：Ubuntu 26.04 LTS（Linux）
> 开发基线：Ubuntu APT Qt 6.10.2
> 参考产品：Telegram 的信息架构和交互原则；不复制其视觉资产、品牌或实现

阅读导航：

- [响应式布局](#6-响应式布局)
- [页面规格](#7-页面规格)
- [消息与同步状态](#8-消息与同步状态的-ui-映射)
- [QML 与 C++ 边界](#10-qml-组件边界)
- [实施里程碑](#14-实施里程碑)
- [服务端兼容矩阵](#16-服务端依赖与兼容矩阵)

## 1. 决策摘要

WIM 客户端采用以下主线：

- Qt 6、Qt Quick、QML、Qt Quick Controls 和 CMake。
- 首个落地平台是 Ubuntu 26.04 LTS，使用系统仓库 Qt 6.10.2；暂不人为设定最低
  操作系统版本。
- 项目以个人开发和开源友好为优先，Qt 使用发行版动态库和开源许可路径，避免依赖
  仅 GPL 或仅商业许可的模块。
- QML 负责视图、布局、动效和输入交互。
- C++ 负责领域状态、列表模型、本地数据库、同步、协议和平台服务抽象。
- UI 第一阶段使用确定性的 Fake Repository，不依赖服务端即可覆盖完整交互状态。
- 服务端接入放在 UI 骨架稳定之后，但 UI 数据模型从第一天就兼容未来的
  `client_message_id`、`message_id`、`conversation_seq` 和
  `user_sync_seq`。
- 真实模式只连接 Auth Gate 返回的 Connection Gateway；客户端已绑定公开
  `tcp_message.proto`，不依赖 Gateway–Message 内部协议或旧 Chat 地址。
- 桌面和移动共享领域模型与基础组件，不强行共享完全相同的页面结构。

当前不创建一个只有静态页面、随后必须推倒重做的原型。第一版即使使用假数据，
也要真实表达离线、本地缓存、发送中、已接受、已送达、已读、未知和失败等状态。

## 2. 规划依据

### 2.1 服务端迁移路径与客户端边界

服务端第一阶段修订方案已确定客户端公开链路为：

```text
Gate HTTP 登录
  -> uid + Connection Gateway endpoint + gateway_id + 短期 token
  -> 连接 Connection Gateway TCP
  -> TLV/8 字节帧头 + Protobuf Packet
  -> Gateway 完成认证、连接绑定、心跳、背压与 TRANSPORT ACK
  -> 业务命令和推送由服务端内部 Gateway×Message 双向流承载
```

`server/gate` 保留为 Auth/API Gate；新增的 `server/gateway` 是唯一客户端长连接
入口；`server/chat` 在迁移期作为 Message 节点实现。客户端不发现 Message 节点，
也不感知 Gateway–Message 的 G×N gRPC 流、节点选择、session lease 或服务端内部
重试。旧 Chat TCP listener 只是服务端切流时的回滚入口，不作为新客户端的兼容目标。

当前服务端源码已经形成登录响应、公开 Packet、错误码、增量同步和业务 ACK 契约。
客户端网络阶段一据此实现，并通过本地假 Gate/Gateway 和可选的真实服务集成测试
验证。客户端只生成公开 `tcp_message.proto`，不生成或绑定
`gateway_message.proto`。

### 2.2 服务端未来路线对客户端的约束

[`设计参考`](../.codex/设计参考.md)、
[`分布式消息实现计划`](../.codex/distributed-message-implementation-plan.md)、
[`Server/Chat 长期演进计划`](../.codex/server-chat-architecture-plan.md) 和
[`服务端路线图`](../mermaid-diagram.svg) 共同确定了以下长期不变量：

1. 启动先读取 SQLite 并立即展示本地数据，网络同步不能阻塞首屏。
2. 实时推送负责低延迟，增量拉取和游标负责完整性。
3. `client_message_id` 用于发送幂等，重试时不可改变。
4. `message_id` 是服务端消息身份，不承担会话排序。
5. `conversation_seq` 是会话内展示、缺口检测和补拉的唯一顺序。
6. `user_sync_seq` 用于好友、会话摘要、撤回和已读等用户维度变更。
7. `ACCEPTED` 只表示服务端已经可靠接受，不表示对方已收到或已读。
8. `delivered_seq` 和 `read_seq` 只能单调推进。
9. 推送允许重复或乱序；客户端落库必须幂等，发现缺口必须补拉。
10. 附件长期使用对象存储，聊天消息只保存不可变附件引用。

第一阶段发送流程为：

```text
本地创建 client_message_id
  -> 立即写入 SQLite outbox 并渲染消息
  -> SendMessage(conversation_id, client_message_id, payload)
  -> Connection Gateway 转发到任一健康 Message 节点
  -> Message 节点原子持久化
  -> ACCEPTED(message_id, conversation_seq)
  -> Message 节点经目标 Connection Gateway 推送
  -> 接收客户端幂等落库
  -> Delivered(conversation_id, conversation_seq)
  -> 用户打开会话
  -> Read(conversation_id, conversation_seq)
```

服务端第一阶段不引入 Kafka 或服务端 Outbox；提交后、推送前的故障由客户端按最后
连续 `conversation_seq` 增量同步补偿。客户端 UI 仍不感知 Gateway、Message 节点
或 G×N 拓扑，只依赖稳定的领域结果和同步游标。

### 2.3 对 Telegram 的取舍

参考 Telegram：

- 会话列表是应用的主要入口。
- 桌面端利用宽屏同时展示导航、列表和当前会话。
- 移动端一次聚焦一个任务，通过返回手势和底部导航切换。
- 本地已有内容优先展示，网络状态不遮挡整个界面。
- 搜索、未读、置顶、归档等高频信息靠近会话列表。
- 消息操作使用上下文菜单，避免在每条消息上长期展示大量按钮。

不照搬 Telegram：

- 不复制其颜色、图标、间距、气泡形状、动效或品牌资源。
- 不沿用 Telegram Desktop 的内部 UI 框架；WIM 使用 Qt Quick/QML。
- 不因为 Telegram 已有频道、机器人、通话、Stories 等能力就提前加入 WIM。
- 不直接依赖 Telegram 的源码或协议。

Telegram 官方说明其 Desktop 客户端是 Qt/C++，并支持 Windows、Linux 和 macOS；
这只作为 Qt 桌面可行性的参考，不作为 WIM 的实现模板：

- <https://telegram.org/apps/desktop>
- <https://github.com/telegramdesktop/tdesktop>

## 3. 产品范围

### 3.1 第一阶段产品目标

第一阶段建立一个可以独立运行和演示的 UI 客户端：

- 登录、注册、验证码和忘记密码页面。
- 本地恢复启动页。
- 会话列表和单聊消息页。
- 联系人、好友申请和添加好友页面。
- 创建群、入群申请和基础群资料页面。
- 个人资料、外观、通知、存储和账号设置页面骨架。
- Light/Dark 主题。
- 桌面键盘、鼠标和移动触屏交互。
- Fake 数据驱动的离线、同步、发送和错误状态。

### 3.2 第一阶段明确不做

- 音视频通话、屏幕共享和直播。
- 端到端加密会话。
- 频道、机器人、Stories、Mini App。
- 消息编辑、复杂撤回策略和定时发送。
- 完整富文本编辑器、Markdown 创作和代码高亮。
- 超大群管理、话题群和管理员权限矩阵。
- 真正的文件上传、媒体转码和对象存储接入。
- 多账号同时在线；UI 可以预留账号切换入口，但不实现运行时语义。

这些能力只有在协议、数据模型和服务端路径确定后再进入独立里程碑。

## 4. UX 原则

### 4.1 Local-first

- 应用启动后立即显示 SQLite 中的会话和消息。
- 同步时使用细粒度状态，不用全屏加载遮住已有数据。
- 网络断开时仍可浏览历史、编辑草稿和创建待发送消息。
- 恢复网络后自动同步，只有需要用户处理的失败才打断用户。

### 4.2 状态诚实

- “发送中”“服务端已接受”“已送达”“已读”必须是不同状态。
- 超时后无法确认结果时显示“待确认”，不能直接显示发送失败。
- 可重试失败和永久失败提供不同操作。
- 本地同步缺口不制造一条伪消息；使用轻量同步提示并自动补齐。

### 4.3 渐进披露

- 首屏只保留会话、联系人、申请和设置等高频入口。
- 消息转发、删除、复制等操作进入长按或右键菜单。
- 群管理和诊断信息进入详情页，不污染聊天主界面。
- 开发模式可以显示 ID、序号和连接状态，发布模式默认隐藏。

### 4.4 平台适配而非像素复制

- 桌面强调空间利用、快捷键、右键菜单、多选和拖放。
- 移动强调单手操作、系统返回手势、长按、分享面板和软键盘避让。
- 领域状态、命令语义和视觉组件共享；窗口结构和导航容器可以不同。

### 4.5 可访问与可测试

- 所有图标按钮都有可访问名称和 tooltip。
- 不只依靠颜色表达消息状态和错误。
- 字体缩放后不截断关键操作。
- 所有动效支持 reduced motion 配置。
- UI 状态必须能通过固定 Fake Scenario 重现。

## 5. 信息架构

第一版顶层导航固定为四项：

| 入口 | 主要内容 | 第一版状态 |
| --- | --- | --- |
| 会话 | 会话列表、搜索、当前聊天 | 核心 |
| 联系人 | 好友列表、用户详情、创建群 | 核心 |
| 申请 | 好友申请、入群申请和审批 | 核心 |
| 设置 | 个人资料、主题、通知、存储、账号 | 骨架 + 部分可用 |

“全部、未读、群聊”等会话筛选放在会话列表顶部，第一版不把它们做成永久顶层
导航。归档、文件、通话等入口暂不加入。

## 6. 响应式布局

断点不是平台判断，而是按窗口可用宽度决定：

| 模式 | 建议宽度 | 页面结构 |
| --- | --- | --- |
| Compact | `< 720` 逻辑像素 | 单页；底部导航；会话列表与聊天页互相导航 |
| Medium | `720–1099` 逻辑像素 | 窄导航栏 + 列表/内容二选一或双栏 |
| Expanded | `>= 1100` 逻辑像素 | 导航栏 + 会话列表 + 当前聊天，详情按需展开 |

具体断点在真机和字体缩放测试后调整，不写死为设备类别。

### 6.1 Expanded 桌面布局

```text
+------+----------------------------+---------------------------------------------+
| Nav  | 会话列表                   | 当前会话                                    |
|      | 搜索                       | 头像 名称 状态          搜索 详情            |
| 会话 | [筛选: 全部 未读 群聊]     +---------------------------------------------+
| 联系 |                            |                                             |
| 申请 | 头像 名称             时间 |               消息时间线                    |
|      | 摘要                  未读 |                                             |
| 设置 |                            |                                             |
|      | 头像 名称             时间 |                                             |
| 头像 | 摘要                       +---------------------------------------------+
|      |                            | +  输入消息...                    发送       |
+------+----------------------------+---------------------------------------------+
```

初始尺寸建议：

- Navigation Rail：64–72 逻辑像素。
- 会话列表：320–380 逻辑像素，可拖动调整但有上下限。
- 当前会话：占据剩余空间，最小 480 逻辑像素。
- 会话详情：320–360 逻辑像素；仅在足够宽时内嵌，否则使用 Drawer/Dialog。

### 6.2 Compact 移动布局

```text
会话列表                            当前聊天
+----------------------+          +----------------------+
| WIM          搜索  + |          | < 头像 名称      ⋮   |
| 全部  未读  群聊     |          +----------------------+
|                      |          |                      |
| 头像 名称       时间 |          |      消息时间线       |
| 摘要            未读 |   --->   |                      |
|                      |          |                      |
| 头像 名称       时间 |          +----------------------+
| 摘要                 |          | + 输入消息...  发送  |
+----------------------+          +----------------------+
| 会话 联系人 申请 设置|          | 软键盘与 safe area   |
+----------------------+          +----------------------+
```

进入聊天页后隐藏底部导航，让消息时间线和输入框获得最大空间。系统返回手势返回原
会话列表位置和滚动位置。

## 7. 页面规格

### 7.1 启动与账号恢复

状态顺序：

```text
Cold Start
  -> 打开本地账号元数据
  -> 有账号：进入主界面并显示本地数据
  -> 后台认证与同步
  -> 无账号：进入登录页
```

启动页只承担非常短的初始化。数据库迁移或恢复时间较长时显示明确步骤，不用无限
转圈。

### 7.2 登录、注册和恢复密码

认证页面共用 `AuthFrame`：

- 品牌区：WIM 名称和简短说明。
- 表单区：明确字段错误、提交状态和服务错误。
- 桌面使用居中卡片；移动使用全屏页面。
- 密码不在页面切换时意外保留。
- 验证码输入支持粘贴和自动移动焦点。

服务端仍缺少长期 refresh token，因此第一版服务接入前需要单独确认账号恢复和
token 生命周期，不把密码长期保存在 SQLite。

### 7.3 会话列表

每个 `ConversationRow` 包含：

- 头像和可选在线标识。
- 名称。
- 最后一条消息摘要；草稿使用独立样式。
- 时间。
- 未读徽标。
- 静音、置顶和发送失败等辅助标识。

第一版排序由 Fake Repository 提供稳定结果。接入服务端后排序依据会话摘要的
服务端版本和时间，不使用 UI 临时排序作为同步事实。

列表状态：

- 有本地数据且正在同步。
- 无本地数据的首次加载。
- 搜索无结果。
- 尚无会话。
- 离线但有缓存。
- 同步失败，可点击重试。

### 7.4 当前会话

页面由四层构成：

1. `ConversationHeader`：返回、头像、名称、状态、搜索、详情。
2. `MessageTimeline`：虚拟列表、日期分隔、未读分隔、加载更早消息。
3. `TransientOverlay`：同步缺口、置顶提示、跳到底部按钮。
4. `MessageComposer`：文本、附件占位、发送按钮、回复上下文。

消息列表不按本地插入时间作为最终顺序。目标排序键固定为：

```text
已接受消息：conversation_seq
尚未接受的本人消息：local_created_at + client_message_id
```

服务端返回 `conversation_seq` 后，模型原地更新消息身份和位置，不能通过删除再插入
造成气泡闪烁。

### 7.5 消息气泡

第一版类型：

- 本人文本。
- 对方文本。
- 系统事件。
- 附件占位。
- 删除/撤回 tombstone 占位。

消息操作：

- 桌面右键或 hover 后显示更多按钮。
- 移动长按打开 bottom sheet。
- 第一版支持复制、回复占位、删除本地、重试。
- 转发、全局删除、编辑在协议确定前隐藏。

### 7.6 联系人与好友申请

联系人页：

- 搜索框。
- 常用入口：添加好友、创建群。
- 按拼音/字母分组的好友列表。
- 点击进入用户详情，主要操作是发消息。

申请页使用统一 `RequestCard`：

- 好友申请：同意、拒绝、查看资料。
- 入群申请：管理员可同意、拒绝。
- 已处理项折叠到历史区域。
- 提交中禁用重复操作；超时后进入待确认而不是立即回滚。

### 7.7 群聊

第一阶段 UI 只覆盖服务端已有或明确规划的边界：

- 创建群。
- 群基本资料。
- 入群申请和审批。
- 群成员列表占位。
- 群消息页面复用 `ConversationPage`。

群文本、退群和成员拉取在服务端未完成前只进入 Fake Scenario，不进入联调验收。

### 7.8 设置

设置分组：

- 个人资料。
- 外观：系统、浅色、深色；字体和紧凑度。
- 通知：总开关和预览占位。
- 数据与存储：缓存大小、自动下载占位。
- 隐私与安全：本地锁、设备列表占位。
- 高级：日志级别和开发诊断，仅开发构建显示。
- 关于：版本、许可证和第三方组件。

## 8. 消息与同步状态的 UI 映射

### 8.1 本人发送消息

| 领域状态 | UI 表现 | 用户操作 |
| --- | --- | --- |
| `DRAFT` | 只在输入框或会话摘要显示草稿 | 编辑、删除 |
| `PENDING_LOCAL` | 气泡立即出现，时钟图标 | 无需等待网络 |
| `WAIT_ACCEPT` | 轻量发送中图标 | 可取消仅限尚未出队 |
| `ACCEPTED` | 单勾或中性确认标识 | 停止 Send 重试 |
| `DELIVERED` | 双勾或可访问文本“已送达” | 无 |
| `READ` | 强调双勾或“已读” | 无 |
| `UNKNOWN` | 空心警告图标，“结果待确认” | 优先同步核对，不生成新 ID |
| `RETRYABLE_FAILED` | 警告图标，“等待重试” | 立即重试或稍后自动重试 |
| `PERMANENT_FAILED` | 红色错误图标 | 修改内容后以新消息发送或删除 |

颜色只作为辅助，图标、tooltip/辅助文本必须同时表达状态。

### 8.2 接收消息

接收流程：

```text
收到推送
  -> 校验并写入 SQLite
  -> 推进连续 synced_through_seq
  -> 写入成功后才能发送 DELIVERED
  -> 当前会话满足已读条件后推进 READ
```

若收到的 `conversation_seq > synced_through_seq + 1`：

- 保留消息但不跳过连续游标。
- UI 在时间线顶部显示非阻塞“正在补齐消息”。
- 自动调度 `SyncMessages`。
- 缺口补齐后移除提示。

### 8.3 全局连接状态

全局状态只在必要时显示：

| 状态 | 表现 |
| --- | --- |
| 在线且已同步 | 不显示常驻提示 |
| 正在连接 | 标题栏细状态文本，不遮挡内容 |
| 离线 | 顶部窄条“离线，可继续浏览和发送” |
| 正在同步 | 会话列表局部进度或细小旋转图标 |
| 同步落后 | 标示正在补齐，不显示错误色 |
| 认证失效 | 明确要求重新登录，保留本地数据 |
| 本地库损坏/迁移失败 | 阻断页面，提供导出日志和重建入口 |

## 9. 视觉系统

WIM 使用独立于 Telegram 的“星空蓝”视觉方向：深夜蓝画布、中性表面与明亮蓝色
强调。整体保持冷静、克制和高信息可读性，不使用过多渐变、发光或装饰性星空元素。

首轮视觉基线：

| 语义 | 浅色主题 | 深色主题 |
| --- | --- | --- |
| Accent | `#315FD6` | `#7EA6FF` |
| Accent Hover/Pressed | `#274EBC` | `#9AB9FF` |
| Accent Container | `#DFE7FF` | `#1D3568` |
| Canvas | `#F5F7FC` | `#0B1020` |
| Surface | `#FFFFFF` | `#121A2B` |
| Outgoing Message | `#E3EAFF` | `#1B315C` |

这些颜色是 UI-0 的设计起点；实现时必须通过对比度测试后才能冻结为正式 token。

第一版使用语义 token，不让业务组件直接写十六进制颜色：

```text
color.accent
color.accentContainer
color.canvas
color.surface
color.surfaceElevated
color.textPrimary
color.textSecondary
color.border
color.success
color.warning
color.error
color.messageIncoming
color.messageOutgoing
```

尺寸 token：

```text
space: 4, 8, 12, 16, 24, 32
radius: 8, 12, 16, full
icon: 16, 20, 24
avatar: 32, 40, 48, 72
```

实现前需要对浅色和深色 token 做 WCAG 对比度检查。气泡圆角、强调色和动效以
WIM 自身设计为准，不复刻 Telegram。

## 10. QML 组件边界

建议的 QML 模块：

```text
Wim.App
  App.qml
  AppRouter.qml

Wim.Design
  Theme.qml
  Tokens.qml
  Typography.qml
  Icons.qml

Wim.Components
  AdaptiveShell.qml
  AppNavigationRail.qml
  AppBottomNavigation.qml
  Avatar.qml
  StatusBadge.qml
  EmptyState.qml
  ErrorBanner.qml
  LoadingRow.qml

Wim.Auth
  AuthFrame.qml
  SignInPage.qml
  SignUpPage.qml
  VerifyCodePage.qml
  ResetPasswordPage.qml

Wim.Chat
  ChatListPage.qml
  ConversationRow.qml
  ConversationPage.qml
  ConversationHeader.qml
  MessageTimeline.qml
  MessageDelegate.qml
  MessageBubble.qml
  MessageComposer.qml
  SyncGapBanner.qml

Wim.Contacts
  ContactListPage.qml
  UserProfilePage.qml
  FriendRequestsPage.qml
  CreateGroupPage.qml

Wim.Settings
  SettingsPage.qml
  AppearancePage.qml
  StoragePage.qml
  AboutPage.qml
```

约束：

- 页面不能直接创建 socket、SQL 查询或 Protobuf 对象。
- QML 不保存唯一业务事实；页面销毁后状态仍在 C++ model/repository。
- 列表只消费 `QAbstractListModel` role。
- QML signal 表达用户意图，例如 `sendRequested(text)`，由 C++ 决定命令是否合法。
- 不使用大量全局 context property；使用明确注册的 QML type/singleton。

## 11. C++ 客户端边界

第一版即使是假数据，也使用最终形态的接口：

```text
Application
  AccountViewModel
  NavigationViewModel
  ConversationViewModel

Domain
  AccountSession
  UserSummary
  ConversationSummary
  Message
  MessageIdentity
  MessagePayload
  MessageDeliveryState
  SyncStatus

Models
  ConversationListModel : QAbstractListModel
  MessageListModel      : QAbstractListModel
  ContactListModel      : QAbstractListModel
  RequestListModel      : QAbstractListModel

Ports
  IAuthRepository
  IConversationRepository
  IMessageRepository
  IContactRepository
  ISyncService
  IPlatformServices

Adapters
  Fake*Repository       # UI 第一阶段
  Sqlite*Repository     # 本地数据阶段
  ConnectionGateway     # 契约冻结后接入 TCP/TLV/Protobuf
  CursorSync            # conversation/user cursor 同步
```

建议的关键类型：

```cpp
struct MessageIdentity {
  std::int64_t clientMessageId{};
  std::optional<std::int64_t> messageId;
  std::optional<std::int64_t> conversationSeq;
};

enum class MessageDeliveryState {
  PendingLocal,
  WaitingAccept,
  Accepted,
  Delivered,
  Read,
  Unknown,
  RetryableFailed,
  PermanentFailed,
};
```

这里仅定义规划意图，不在 UI 里绑定当前 `Packet.seq` 的临时复用语义。

## 12. 建议目录

进入实现阶段后新增独立 `client/`，不放入 `server/test`：

```text
client/
  CMakeLists.txt
  cmake/
  src/
    app/
    domain/
    models/
    ports/
    adapters/
      fake/
      sqlite/
      connection_gateway/
      sync/
    platform/
      android/
      ios/
      windows/
      linux/
  qml/
    Wim/App/
    Wim/Design/
    Wim/Components/
    Wim/Auth/
    Wim/Chat/
    Wim/Contacts/
    Wim/Settings/
  resources/
    icons/
    fonts/
  tests/
    unit/
    qml/
    screenshots/
```

客户端只共享协议定义和确有必要的纯领域工具，不链接服务端数据库、Redis、日志、
RPC pool 或 `server/public` 整体。

## 13. Fake Scenario 设计

UI 开发不使用零散硬编码数组，使用可切换场景：

| 场景 | 目的 |
| --- | --- |
| `normal` | 常规会话、联系人和消息 |
| `empty-account` | 新账号空状态 |
| `offline-cached` | 离线但有本地数据 |
| `first-bootstrap` | 首次同步和骨架加载 |
| `dense-chat-list` | 大量会话、长名称和多未读 |
| `send-lifecycle` | 一条消息走完整状态机 |
| `send-unknown` | 超时后待确认和同步核对 |
| `sync-gap` | 收到跳号推送并补拉 |
| `auth-expired` | 本地数据可见但要求重新认证 |
| `friend-requests` | 多种申请状态和重复提交保护 |
| `group-admin` | 入群审批和群成员占位 |
| `long-content` | 长文本、emoji、中文、英文和换行 |
| `large-history` | 2000 条附加历史消息的模型装载和 ListView 虚拟化 |

场景必须可由命令行参数或开发设置切换，便于截图测试和回归。

## 14. 实施里程碑

### UI-0：工程骨架与设计系统

交付：

- `client/` CMake 工程。
- QML 模块、Theme、Tokens、Typography、Icons。
- Fake Repository 和 Scenario 切换。
- Linux Desktop 可运行；Android 至少能构建空壳。

退出门槛：Light/Dark、三种窗口宽度和字体缩放都能稳定运行。

### UI-1：桌面会话主流程

交付：

- Expanded/Medium shell。
- 会话列表、消息时间线、输入框、详情 Drawer。
- 发送状态全生命周期演示。
- 键盘导航、右键菜单、未读跳转和列表滚动恢复。

退出门槛：不连接服务端即可完整演示“本地创建消息到已读”的所有 UI 状态。

### UI-2：移动布局

交付：

- Compact shell 和底部导航。
- 会话页 push/pop、系统返回、safe area、软键盘避让。
- 长按菜单、触摸目标尺寸和旋转适配。

退出门槛：同一 Fake Scenario 在 Android 和桌面窄窗口上行为一致，不出现桌面
控件简单缩小后的布局。

### UI-3：认证、联系人、申请和设置

交付：

- 完整认证页面状态。
- 联系人、用户资料、好友申请、创建群和审批页面。
- 外观、通知、存储和关于页面。

退出门槛：顶层信息架构闭环，所有空状态、加载状态和错误状态可重现。

### DATA-1：SQLite 与本地恢复

交付：

- 每账号独立 SQLite。
- conversations、messages、outbox、sync_state 等最小表。
- 启动从本地恢复，数据库写入与游标推进使用同一事务。
- Fake Repository 可替换为 SQLite Repository，QML 无改动。

### DATA-2：迁移与规模门禁

交付：

- 使用 `PRAGMA user_version` 逐版本、单事务升级 schema。
- 当前 schema v3 在 v2 索引基础上增加远端 conversation ID 映射。
- v1→v2→v3 自动迁移保留账号数据；高于当前版本的数据库拒绝降级打开且不改写。
- 2000 条入站批次重复应用仍保持幂等，游标与消息在同一事务提交。
- `large-history` 场景和离屏 UI smoke 覆盖 2000 条附加历史消息。

退出门槛：新库、已有 v1 库和未来版本库都有确定性行为；规模场景的 repository、
model 和 QML 首帧均进入自动化回归。

### NET-1：客户端公开契约冻结

开始条件（由服务端里程碑 2～3 提供）：

- Auth Gate 返回 Connection Gateway IP、TCP 端口、`gateway_id` 和短期 token。
- 客户端 TCP/TLV/Protobuf 的帧头、登录、心跳、错误码和版本协商冻结。
- 文本发送明确 `client_message_id` 幂等、`ACCEPTED(message_id,
  conversation_seq)` 和未知结果的查询/重试规则。
- 增量同步、业务 DELIVERED/READ 与本地连续游标契约冻结。

当前状态：已按当前服务端源码完成公开契约适配。客户端自有 Service ID/ErrorCode
镜像，并通过 QtProtobuf 只从 `tcp_message.proto` 生成类型；客户端不再链接 Google
`libprotobuf`，`gateway_message.proto` 仍严格留在 Gateway–Message 内部。真实 Gate
登录、Gateway 鉴权、单聊接受/推送、三类 ACK 和会话增量同步已有 live network test
证据；Qt/Google Protobuf 黄金线格式兼容测试覆盖 `optional`、枚举和重复消息。

### NET-2：Connection Gateway 与同步接入

交付：

- Auth Gate adapter 与 Connection Gateway 会话状态机。
- TCP/TLV 分帧、Protobuf、认证、心跳、重连、退避和本地 backpressure。
- 文本发送 outbox 与持久 `ACCEPTED` 闭环。
- `ListConversations`、`SyncMessages` 或等价接口及 `user_sync_seq`。
- 缺口检测、重复去重、累计 delivered/read cursor 和 cursor expired 重建。

退出门槛：实时推送丢失、乱序、重复或进程重启后，客户端均能依靠 SQLite 和同步
游标恢复；两个客户端连接不同 Gateway 时可完成单聊闭环。未冻结能力保持 feature
flag 关闭。

阶段一完成状态（2026-07-17）：

- 已实现 Auth Gate 登录/注册/验证码/密码恢复 HTTP 请求 adapter。
- 已实现 Gateway TCP/TLV/Protobuf 登录、心跳、退出、指数退避重连、请求超时和
  同响应类型串行背压。
- 已实现服务端当前注册的好友、申请、文本、同步、群关系、群文本、文件上传和三类
  ACK 请求；未注册的 Service ID 不暴露为可用功能。
- 已把登录、好友/申请、单聊文本、持久 ACCEPTED、入站推送 ACK、远端会话映射和
  增量同步接入 AppController 与 SQLite schema v3。
- 登录页已接入验证码、注册、登录和找回密码；群创建后建立本地会话，群文本发送、
  重连恢复和打开会话后的 READ ACK 已接入 AppController。
- 本地假 Gate/Gateway 覆盖 12 类业务请求、推送/ACK 和重连再认证；真实服务测试已
  覆盖核心单聊闭环，双 Gateway 故障注入仍作为后续独立目标。

### ANDROID-0：构建与部署基线

- 首 ABI 固定为 `arm64-v8a`，使用 Qt 6.10.2 Android kit、JDK 17 和 NDK
  `27.2.12479018`。
- QtProtobuf 由 Qt Android kit 提供目标运行库，宿主机只运行代码生成工具。
- 先完成 APK 安装、Compact UI、SQLite/QML/QtProtobuf 装载，再接真机网络。
- 真机 Gate URL 和 State 返回的 Gateway advertised host 都必须是设备可达地址。
- 具体安装、构建、`adb` 验证和 AAB 门禁见
  [`android-build-deployment-plan.md`](android-build-deployment-plan.md)。

### PLATFORM-1：平台服务

- Android：FCM、Keystore、系统分享、通知渠道。
- iOS：APNs、Keychain、通知权限和后台恢复。
- Windows：Credential Manager、Toast、托盘和任务栏。
- Linux：freedesktop Notifications 的 Qt DBus adapter、设置页自检、应用图标与
  `.desktop` 安装入口已完成；Secret Service 和托盘仍待后续闭环。

移动后台不维持“永不掉线”的 TCP 幻觉。系统推送负责唤醒/提醒，恢复后始终通过
同步游标核对完整数据。

## 15. UI 第一阶段验收矩阵

### 15.1 尺寸

- 390 × 844：典型手机竖屏。
- 844 × 390：手机横屏。
- 768 × 1024：平板/窄桌面。
- 1280 × 800：普通桌面。
- 1920 × 1080：宽桌面。
- 200% 字体/显示缩放。

### 15.2 输入

- 鼠标、触控板、键盘。
- 触摸、长按、滑动返回。
- 输入法组合输入，不在 composing 阶段错误发送。
- 中文、英文、emoji、多行文本和长词。

### 15.3 状态

- 每个页面覆盖 loading、content、empty、offline、recoverable error 和 fatal error。
- 消息覆盖全部发送状态。
- 列表覆盖插入、更新、删除、排序和滚动位置保持。
- 主题切换和窗口变窄不丢失当前会话与草稿。

### 15.4 自动化

- C++ 领域状态和 model 单元测试。
- QML 页面 smoke test。
- 固定 Fake Scenario 截图回归。
- `qmllint`、QML module/import 检查。
- 无显示服务器的 CI 使用 `QT_QPA_PLATFORM=offscreen` 运行 `qmltestrunner`。
- `git diff --check`。

## 16. 服务端依赖与兼容矩阵

| 客户端能力 | 当前服务端 | 目标服务端 | 规划处理 |
| --- | --- | --- | --- |
| 注册/登录 | Auth Gate 返回 Gateway + token | 返回 Connection Gateway + token | 验证码、注册、登录、找回密码 UI 已接入 |
| 长连接登录 | Connection Gateway 已成型 | Connection Gateway TCP/TLV/Protobuf | 已接入；不接旧 Chat 路径 |
| 单聊发送 | 持久幂等闭环已成型 | client id + conversation id | outbox 与请求已接入，核心 live test 已验证 |
| 发送结果 | ACCEPTED + conversation seq | 持久 ACCEPTED + conversation seq | SQLite 事务闭环已接入 |
| 会话列表 | 缺少稳定目标接口 | ListConversations/user sync | UI 使用 Fake/SQLite |
| 历史同步 | conversation cursor 已提供 | conversation cursor | 按会话增量同步和 SQLite 游标已接入 |
| 好友/申请 | Gateway handler 已注册 | 用户变更日志 | 拉取、发起、回复和推送已接入 |
| 群聊 | 创建/审批/文本已注册 | conversation 统一模型 | 创建、审批、群文本闭环已接入，完整群资料 UI 后续 |
| 文件 | 上传已实现，发送仍为空实现 | 预签名 URL + 对象存储 | 保留请求层，第一版不提供伪附件闭环 |
| 多设备 | 未完成 | device cursor/session directory | 只保留领域扩展点 |
| 移动推送 | 未完成 | APNs/FCM + 同步恢复 | PLATFORM-1 |

## 17. 风险与门禁

### 17.1 Qt 版本和许可

本轮已经确定：

- 首要平台为 Linux，开发环境是 Ubuntu 26.04 LTS。
- 使用 Ubuntu APT 已安装的 Qt 6.10.2：`qt6-base-dev`、
  `qt6-declarative-dev` 和 `qt6-svg-dev`。
- 不人为承诺最低操作系统版本；扩展到新平台时以当时 Qt 和平台工具链支持矩阵为准。
- 个人项目优先使用 Qt 开源许可路径和动态链接，跟踪实际使用模块的 LGPL/GPL 许可，
  避免无意引入仅 GPL 或仅商业许可模块。
- 若以后发布闭源商店版本，再单独审查 LGPL 重链接要求、商店条款和商业许可需求。

Qt 6.10 是稳定版本但不是 Qt 上游 LTS。当前上游 LTS 是 Qt 6.8，而即时 LTS 补丁
主要面向商业客户。对当前个人项目，发行版维护的 Qt 6.10.2 比另装一套上游 LTS
更符合开源友好和环境可重复原则。客户端仍避免依赖某个 Qt 6 小版本的实验特性，
保持未来升级到后续 Qt 6 版本的源码兼容性。

### 17.2 迁移期协议漂移风险

服务端旧 Chat 路径与新 Connection Gateway 路径会短期共存，但新客户端不做双轨
联调。NET-1 门禁通过后，以 `ConnectionGateway` 和 `CursorSync` adapter 隔离传输，
禁止 QML 页面出现 service id、`Packet`、token、`gateway_id` 或 gRPC 内部帧概念。

### 17.3 UI 过度设计风险

第一版不实现超出服务端路线的社交功能。每个页面和组件必须对应已确认用户流程、
明确未来协议，或用于验证核心交互状态。

### 17.4 Telegram 参考边界

若后续需要研究 Telegram 源码，只针对一个明确问题做局部阅读，例如消息列表
虚拟化、富文本布局、快捷键或媒体查看器。完整仓库不是 WIM 的依赖，也不作为开始
UI 实现的前置条件。

## 18. 下一步

截至本版，UI-0～UI-3、DATA-1～DATA-2、NET-1 的公开协议适配和 Linux 桌面入口已
完成首轮实现和自动化验证。接下来按以下顺序推进：

1. 安装 Qt 6.10.2 Android arm64 kit、JDK 和 SDK/NDK，产出可安装的首个 APK。
2. 完善 Android 系统返回、safe area、软键盘和应用前后台恢复；开发服务器地址入口
   已完成。
3. 在已验证安装和冷启动的 arm64 真机上完成双向文本、断线补同步与重启恢复验收。
4. 继续补齐第一阶段尚未完整落地的用户资料、申请、群资料、设置子页和消息操作界面。
5. 服务端测试稳定后补双 Gateway 故障注入；客户端并行继续分页时间线和 Linux 托盘。

Fake 与 SQLite 模式继续作为 UI 开发、截图回归和本地状态机验证基线；正式 token
契约稳定后再接 Android Keystore/桌面 Secret Service，避免以演示凭据代替安全设计。
