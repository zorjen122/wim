# wim 功能清单与验证状态

说明：本文档基于当前本地代码树和本轮已经跑通的 smoke 流程整理。`已验证` 表示已经通过脚本或端到端 smoke 断言；`未验证` 表示代码入口存在但本轮未做端到端断言，或当前实现仍是骨架、未注册、未完整接通。

## 总功能清单

| 编号 | 功能 | 状态 | 主要索引 |
| --- | --- | --- | --- |
| V1 | CMake/protobuf/gRPC 本地构建 | 已验证 | [构建脚本](../scripts/build.sh#L11) |
| V2 | MySQL 表结构初始化与测试数据导入 | 已验证 | [初始化脚本](../scripts/init_mysql.sh#L13) |
| V3 | Redis 与本地服务编排启动 | 已验证 | [本地服务脚本](../scripts/run_local_services.sh#L60) |
| V4 | 验证码请求：gate -> verify -> Redis | 已验证 | [gate 验证码入口](../server/gate/src/service/Service.cc#L155) |
| V5 | 注册：gate 写入 MySQL 用户 | 已验证 | [注册处理](../server/gate/src/service/Service.cc#L176) |
| V6 | 登录：gate 校验用户并向 state 获取 chat 节点 | 已验证 | [登录处理](../server/gate/src/service/Service.cc#L229) |
| V7 | chat TCP 登录初始化与在线用户映射 | 已验证 | [chat 登录处理](../server/chat/src/service/Service.cc#L388) |
| V8 | 离线文本消息落库 | 已验证 | [离线消息路径](../server/chat/src/service/Service.cc#L352) |
| V9 | 文件上传：chat -> file gRPC -> 本地文件 | 已验证 | [chat 文件上传处理](../server/chat/src/service/Service.cc#L204) |
| V10 | 同节点在线文本直推 | 已验证 | [本地在线发送](../server/chat/src/service/Service.cc#L338) |
| V11 | 跨 chat 节点文本 RPC 转发 | 已验证 | [跨节点转发判断](../server/chat/src/service/Service.cc#L303) |
| V12 | 好友申请与好友申请列表拉取 | 已验证 | [relationship smoke 好友申请](../scripts/smoke_relationships.sh#L214) |
| V13 | 好友申请回复与好友列表拉取 | 已验证 | [relationship smoke 好友回复](../scripts/smoke_relationships.sh#L222) |
| V14 | 单会话消息拉取与用户消息拉取 | 已验证 | [relationship smoke 消息拉取](../scripts/smoke_relationships.sh#L248) |
| V15 | 创建群组 | 已验证 | [relationship smoke 创建群组](../scripts/smoke_relationships.sh#L269) |
| V16 | 申请/审批加入群组 | 已验证 | [relationship smoke 入群审批](../scripts/smoke_relationships.sh#L277) |
| U1 | gate 健康检查 `/test` | 未验证 | [路由注册](../server/gate/src/service/Service.cc#L50) |
| U2 | gate/state 网络探测 `/test-net` | 未验证 | [网络探测路由](../server/gate/src/service/Service.cc#L57) |
| U3 | 忘记密码/重置密码 | 未验证 | [重置密码处理](../server/gate/src/service/Service.cc#L324) |
| U4 | chat 故障转移/心跳异常后重新分配节点 | 未验证 | [arrhythmia 接口](../server/gate/src/service/Service.cc#L302) |
| U5 | HTTP 初始化用户资料 | 未验证 | [未注册处理函数](../server/gate/src/service/Service.cc#L275) |
| U6 | chat 用户退出清理 | 未验证 | [退出处理](../server/chat/src/service/Service.cc#L369) |
| U7 | 心跳、ACK 与重传定时器 | 未验证 | [心跳处理](../server/chat/src/service/Service.cc#L161) |
| U8 | 重复消息去重 | 未验证 | [文本去重](../server/chat/src/service/Service.cc#L292) |
| U9 | 查找用户 | 未验证 | [查找用户函数](../server/chat/src/service/Service.cc#L181) |
| U13 | 删除好友 | 未验证 | [协议枚举](../server/public/include/Const.h#L118) |
| U14 | 点对点文件发送 | 未验证 | [空实现](../server/chat/src/service/Service.cc#L269) |
| U17 | 群退出、群文本、群成员/消息拉取 | 未验证 | [群文本空实现](../server/chat/src/service/Group.cc#L278) |
| U18 | state 备用 chat 节点激活 | 未验证 | [备用节点激活](../server/state/src/service/service.cc#L63) |
| U19 | gate 登录入口下的多 chat 节点轮询路由 | 未验证 | [state 轮询选择](../server/state/src/service/service.cc#L38) |
| U20 | verify 真实邮件发送 | 未验证 | [邮件发送分支](../server/verify/server.js#L49) |
| U21 | Kafka 消息生产 | 未验证 | [KafkaProducer](../server/chat/src/util/KafkaOperator.cc#L3) |

## 已验证功能

| 编号 | 预期行为 | 代码索引 | 测试方法 |
| --- | --- | --- | --- |
| V1 | 在 `build/wim` 下完成 server 总工程配置和编译，生成 gate/state/file/chat/test 可执行文件。 | [构建脚本](../scripts/build.sh#L11)、[server 总 CMake](../server/CMakeLists.txt#L15) | 执行 `./scripts/build.sh`；本轮已验证成功。 |
| V2 | 创建 `chatServ` 数据库、必须数据表，并导入本地 smoke 使用的用户、好友、消息、群测试数据。 | [初始化脚本](../scripts/init_mysql.sh#L13)、[表结构](../server/chat/init_mysql_test.sql#L24)、[种子数据](../server/chat/init_mysql_test.sql#L115) | 执行 `./scripts/init_mysql.sh`，确认 `users=2`、`messages=2`，或手动查询 `chatServ.users/messages`。 |
| V3 | 自动启动项目 Redis，并依次拉起 verify、state、file、一个或多个 chat、gate 服务；退出脚本时清理本脚本启动的服务。 | [Redis 启动](../scripts/start_redis.sh#L25)、[服务启动顺序](../scripts/run_local_services.sh#L60)、[多 chat 配置循环](../scripts/run_local_services.sh#L63) | 执行 `./scripts/run_local_services.sh`，再运行 smoke；本轮单节点和双节点模式均跑通。 |
| V4 | 客户端向 gate 提交邮箱，gate 通过 gRPC 调 verify；verify 生成或复用 Redis 验证码，本地模式跳过真实邮件发送，返回 `error: 0`。 | [gate 路由](../server/gate/src/service/Service.cc#L70)、[gate handler](../server/gate/src/service/Service.cc#L155)、[verify gRPC client](../server/gate/src/service/Service.cc#L19)、[verify 服务](../server/verify/server.js#L15)、[本地跳过邮件](../server/verify/server.js#L49)、[smoke 断言](../scripts/smoke.sh#L50) | 服务启动后执行 `curl -H 'Content-Type: application/json' -d '{"email":"x@example.com"}' http://127.0.0.1:18080/post-verifycode`，或运行 `./scripts/smoke.sh`。 |
| V5 | 注册时检查邮箱和用户名唯一性，生成 uid，写入 `users` 表，并返回 uid/username/email。 | [注册 handler](../server/gate/src/service/Service.cc#L176)、[唯一性检查](../server/gate/src/service/Service.cc#L192)、[uid 生成](../server/gate/src/service/Service.cc#L205)、[smoke 注册](../scripts/smoke.sh#L55) | 运行 `./scripts/smoke.sh`，或手动 POST `/post-signUp` 后查询 `chatServ.users`。 |
| V6 | 登录时校验用户名密码，调用 state 获取 chat IP/port，并返回 `uid/ip/port/init`。 | [登录 handler](../server/gate/src/service/Service.cc#L229)、[StateClient::GetImServer](../server/gate/src/service/StateClient.cc#L28)、[state 分配节点](../server/state/src/service/service.cc#L38)、[smoke 登录断言](../scripts/smoke.sh#L60) | 运行 `./scripts/smoke.sh`；脚本断言返回 `127.0.0.1:8090`。 |
| V7 | 客户端连接 chat TCP 后发送 `ID_LOGIN_INIT_REQ`，chat 读取用户资料并把 uid 映射到本地 session 与 Redis 在线信息，返回登录初始化响应。 | [imTest 连接](../server/test/src/service/main.cc#L84)、[客户端登录请求](../server/test/src/service/chat.cc#L319)、[服务注册](../server/chat/src/service/Service.cc#L59)、[OnLogin](../server/chat/src/service/Service.cc#L388)、[OnlineUser::MapUser](../server/chat/src/util/OnlineUser.cc#L23) | `./scripts/smoke.sh` 和 `./scripts/smoke_multi_chat.sh` 均等待接收端日志出现 `ID_LOGIN_INIT_RSP`。 |
| V8 | 当接收者不在线时，chat 将文本消息写入 `messages` 表，响应发送者成功。 | [TextSend 入口](../server/chat/src/service/Service.cc#L278)、[离线落库](../server/chat/src/service/Service.cc#L352)、[客户端发送文本](../server/test/src/service/chat.cc#L539)、[smoke 查询落库](../scripts/smoke.sh#L68) | 运行 `./scripts/smoke.sh`；脚本向离线用户 1002 发消息并查询 MySQL 中对应 content 计数为 1。 |
| V9 | 客户端把文件按块发给 chat，chat 通过 FileRpc 转发到 file 服务，file 服务保存到 `server/file/<uid>/<filename>`。 | [客户端上传](../server/test/src/service/chat.cc#L664)、[chat UploadFile](../server/chat/src/service/Service.cc#L204)、[FileRpc 转发](../server/chat/src/service/FileRpc.cc#L27)、[file Upload](../server/file/src/service/Service.cc#L75)、[smoke 文件比对](../scripts/smoke.sh#L88) | 运行 `./scripts/smoke.sh`；脚本比对源文件和 `server/file/1001/<filename>` 内容一致。 |
| V10 | 接收者在线且在同一个 chat 节点时，chat 直接通过本地 session 向接收者推送文本消息。 | [本地在线分支](../server/chat/src/service/Service.cc#L338)、[服务端重传发送](../server/chat/src/util/OnlineUser.cc#L73)、[smoke 接收端](../scripts/smoke.sh#L106)、[smoke 在线发送](../scripts/smoke.sh#L127) | 运行 `./scripts/smoke.sh`；脚本保持 1002 在线，1001 发送消息，并断言接收端日志包含 payload。 |
| V11 | 接收者在线但在另一个 chat 节点时，发送方节点根据 Redis 在线信息走 chat gRPC 转发，目标节点复用 TextSend 本地发送。 | [跨节点判断](../server/chat/src/service/Service.cc#L303)、[ImRpc 转发](../server/chat/src/service/ImRpc.cc#L68)、[目标 RPC 服务](../server/chat/src/service/RpcService.cc#L75)、[双节点 state 配置](../server/conf/state-multi.yaml#L5)、[双节点 smoke](../scripts/smoke_multi_chat.sh#L27) | 先用 `WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" ./scripts/run_local_services.sh` 启动，再运行 `./scripts/smoke_multi_chat.sh`。 |
| V12 | 发起好友申请时写入 `friendApplys`，接收方能拉取待处理申请。 | [NotifyAddFriend](../server/chat/src/service/Friend.cc#L29)、[好友申请拉取](../server/chat/src/service/Service.cc#L451)、[relationship smoke 好友申请](../scripts/smoke_relationships.sh#L214)、[数据库断言](../scripts/smoke_relationships.sh#L314) | 启动单节点服务后运行 `./scripts/smoke_relationships.sh`；脚本断言申请响应、申请列表和两条镜像申请记录。 |
| V13 | 回复好友申请时更新申请状态，接受后插入双向好友关系，好友列表能拉到对方资料。 | [ReplyAddFriend](../server/chat/src/service/Friend.cc#L105)、[存储回复](../server/chat/src/service/Friend.cc#L182)、[好友列表拉取](../server/chat/src/service/Service.cc#L479)、[relationship smoke 好友回复](../scripts/smoke_relationships.sh#L230)、[数据库断言](../scripts/smoke_relationships.sh#L319) | 运行 `./scripts/smoke_relationships.sh`；脚本断言回复响应、好友列表包含对方、`friendApplys.status=1` 且 `friends` 双向记录存在。 |
| V14 | 单会话消息拉取按 `from/to` 返回消息；用户消息拉取按接收者 uid 返回消息。 | [单会话消息拉取](../server/chat/src/service/Service.cc#L512)、[用户消息拉取](../server/chat/src/service/Service.cc#L550)、[relationship smoke 消息夹具](../scripts/smoke_relationships.sh#L78)、[relationship smoke 响应断言](../scripts/smoke_relationships.sh#L248) | 运行 `./scripts/smoke_relationships.sh`；脚本插入唯一消息后通过两个拉取接口断言 payload 出现在响应中。 |
| V15 | 创建群组时生成 gid/sessionKey，写入 `groupInfo`，并把创建者写为群主成员。 | [GroupCreate](../server/chat/src/service/Group.cc#L15)、[写入 group](../server/chat/src/service/Group.cc#L31)、[写入 master](../server/chat/src/service/Group.cc#L41)、[relationship smoke 创建群组](../scripts/smoke_relationships.sh#L269)、[数据库断言](../scripts/smoke_relationships.sh#L329) | 运行 `./scripts/smoke_relationships.sh`；脚本断言创建响应返回 gid，并确认 `groupInfo` 与创建者成员记录存在。 |
| V16 | 申请加入群组时写入 `groupApplys`；管理员审批通过后更新申请并插入成员。 | [GroupNotifyJoin](../server/chat/src/service/Group.cc#L93)、[GroupReplyJoin](../server/chat/src/service/Group.cc#L206)、[relationship smoke 入群申请](../scripts/smoke_relationships.sh#L277)、[relationship smoke 审批](../scripts/smoke_relationships.sh#L285)、[数据库断言](../scripts/smoke_relationships.sh#L334) | 运行 `./scripts/smoke_relationships.sh`；脚本验证申请响应、审批响应、`groupApplys.status=1` 和两名群成员记录。 |

## 未验证功能

| 编号 | 预期行为 | 代码索引 | 测试方法 |
| --- | --- | --- | --- |
| U1 | `GET /test` 应返回简单健康检查字符串 `[TEST]`。 | [路由注册](../server/gate/src/service/Service.cc#L50) | 启动 gate 后执行 `curl http://127.0.0.1:18080/test`，断言响应体为 `[TEST]`。 |
| U2 | `GET /test-net` 应通过 gate 调 state，再由 state 探测/激活 chat RPC，成功时返回 `[TEST SUCCESS!]`。当前默认配置下没有完整 backup 场景断言。 | [gate 路由](../server/gate/src/service/Service.cc#L57)、[StateClient::TestNetworkPing](../server/gate/src/service/StateClient.cc#L68)、[state TestNetworkPing](../server/state/src/service/service.cc#L99) | 准备包含 backup chat 的 state 配置，启动服务后 `curl http://127.0.0.1:18080/test-net`，并断言返回值与 chat 端日志。 |
| U3 | 忘记密码应校验 Redis 验证码，并把用户密码更新为请求中的新密码。当前 handler 没有从请求读取新密码后再更新，存在实现风险。 | [forgetPassword](../server/gate/src/service/Service.cc#L324)、[验证码校验](../server/gate/src/service/Service.cc#L334)、[密码更新调用](../server/gate/src/service/Service.cc#L348) | 先设置 Redis 验证码，再 POST `/post-forget-password`，随后用新旧密码分别登录确认结果；预期需要先补齐新密码字段逻辑。 |
| U4 | 心跳异常或 chat 不可用时，客户端应触发故障转移，gate/state 重新分配可用 chat 节点。当前客户端只记录 arrhythmia，未真正重连。 | [gate arrhythmia](../server/gate/src/service/Service.cc#L302)、[客户端心跳超时](../server/test/src/service/chat.cc#L347)、[客户端 arrhythmiaHandle](../server/test/src/service/chat.cc#L381)、[state 激活 backup](../server/state/src/service/service.cc#L63) | 启动带 backup 的双节点环境，断开当前 chat，观察客户端是否调用 gate 并重连到新节点；当前应作为待实现验证。 |
| U5 | HTTP 初始化用户资料应接收 uid/name/age/sex/headImageURL 并写入 `userInfo`。当前函数存在，但没有在 gate 构造函数中注册路由。 | [initUserinfo 函数](../server/gate/src/service/Service.cc#L275)、[当前已注册路由列表](../server/gate/src/service/Service.cc#L70) | 直接 POST 目前会 404；若决定保留该 HTTP 功能，应注册路由后再用 curl 和 MySQL 查询验证。 |
| U6 | 用户退出时应删除 Redis 在线信息、清理 session 映射和 ACK 定时器。smoke 中客户端会发送 `q`，但未断言 Redis/session 清理结果。 | [客户端 quit](../server/test/src/service/chat.cc#L331)、[服务端 UserQuit](../server/chat/src/service/Service.cc#L369)、[OnlineUser::ClearUser](../server/chat/src/util/OnlineUser.cc#L45) | 登录后发送 `q`，再用 `redis-cli` 检查对应 `im:user:<uid>` 已删除，并观察 chat 日志无残留重传。 |
| U7 | 客户端心跳应发送 `ID_PING_REQ`，服务端回 `ID_PING_RSP` 并维护重传；推送消息收到 ACK 后应取消服务端定时器。当前 imTest 默认没有启动 `OnheartBeat`。 | [客户端 ping](../server/test/src/service/chat.cc#L337)、[默认未启用心跳](../server/test/src/service/main.cc#L100)、[服务端 PingHandle](../server/chat/src/service/Service.cc#L161)、[AckHandle](../server/chat/src/service/Service.cc#L170)、[Pong 重传](../server/chat/src/util/OnlineUser.cc#L136) | 打开心跳或增加专用测试客户端，断言 ping/pong、ACK 取消、超时清理三种路径。 |
| U8 | 同一用户同一客户端 seq 重复发送时，应返回重复消息错误并避免重复落库或重复上传。 | [文本去重](../server/chat/src/service/Service.cc#L292)、[上传去重](../server/chat/src/service/Service.cc#L215) | 写一个固定 seq 的测试客户端，连续发送两次文本/文件块，断言第二次返回 `RepeatMessage` 且 MySQL/文件没有重复副作用。 |
| U9 | 查找用户应按 username 查询用户与用户资料并返回 profile。当前 `SerachUser` 函数存在，但没有在 `Service::Init` 注册。 | [SerachUser](../server/chat/src/service/Service.cc#L181)、[Service::Init 注册列表](../server/chat/src/service/Service.cc#L55)、[客户端 searchUser](../server/test/src/service/chat.cc#L394) | 当前发送 `ID_SEARCH_USER_REQ` 应走 NotFound；若需要该功能，应注册 handler 后用 imTest 或专用客户端断言返回资料。 |
| U13 | 删除好友应删除双方好友关系并处理会话影响。当前只有协议枚举，没有服务端 handler 或客户端命令。 | [协议 ID](../server/public/include/Const.h#L118)、[当前注册列表缺失](../server/chat/src/service/Service.cc#L55) | 需要先实现 handler 和客户端入口；之后构造好友关系，发送删除请求并查询 `friends` 表。 |
| U14 | 点对点文件发送应把文件块推给接收者或存储离线文件消息。当前服务端 `FileSend` 是空实现。 | [FileSend 空实现](../server/chat/src/service/Service.cc#L269)、[客户端 sendFile](../server/test/src/service/chat.cc#L706)、[协议注册](../server/chat/src/service/Service.cc#L83) | 当前执行 `sendFile` 只会得到空响应；实现后应覆盖在线直推、跨节点转发、离线存储三种场景。 |
| U17 | 群退出应删除成员；群文本应持久化并向群成员 fan-out；群成员/群消息拉取应返回列表。当前群退出和群文本为空实现，群成员拉取有客户端入口但服务端未注册。 | [GroupQuit 空实现](../server/chat/src/service/Group.cc#L266)、[GroupTextSend 空实现](../server/chat/src/service/Group.cc#L278)、[群成员拉取客户端](../server/test/src/service/chat.cc#L594)、[Service::Init 未注册群成员拉取](../server/chat/src/service/Service.cc#L78) | 需要先补齐服务端 handler；之后用两客户端和 MySQL 查询验证退出、发送、拉取三类行为。 |
| U18 | state 应能从 backup chat 中选择一个，调用其 RPC 激活，并把它切为 active。当前 smoke 未覆盖 backup 配置。 | [ActiveImBackupServer](../server/state/src/service/service.cc#L63)、[state 构造 backup RPC](../server/state/src/service/service.cc#L29)、[chat ActiveService RPC](../server/chat/src/service/RpcService.cc#L19) | 准备包含 `status: backup` 的配置，调用 state gRPC 或 gate 故障转移入口，断言 backup chat 被激活且后续登录可路由过去。 |
| U19 | 多 chat 节点模式下，用户通过 gate 登录时应由 state 在 active 节点间轮询分配。当前双节点 smoke 是手动连接 8090/8091 验证转发，没有断言 gate 登录轮询。 | [state 轮询分配](../server/state/src/service/service.cc#L38)、[双节点 state 配置](../server/conf/state-multi.yaml#L20)、[双节点 smoke 手动端口](../scripts/smoke_multi_chat.sh#L27) | 双节点启动后连续 POST `/post-signIn` 多次，断言返回端口在 8090/8091 间按预期轮换。 |
| U20 | verify 服务在非本地模式下应调用邮件模块发送真实验证码邮件。当前本地运行固定设置 `WIM_VERIFY_SEND_EMAIL=0`，只验证了跳过发送分支。 | [邮件发送分支](../server/verify/server.js#L49)、[本地启动设置](../scripts/run_local_services.sh#L60) | 配置真实 SMTP/授权码，取消 `WIM_VERIFY_SEND_EMAIL=0`，执行 `/post-verifycode` 并确认邮箱收到验证码。 |
| U21 | KafkaProducer 应按配置连接 Kafka，并向 `im-messages` topic 发送消息。当前代码存在但业务路径未调用，smoke 未启动 Kafka。 | [KafkaProducer 构造](../server/chat/src/util/KafkaOperator.cc#L3)、[Produce](../server/chat/src/util/KafkaOperator.cc#L18)、[SaveMessage](../server/chat/src/util/KafkaOperator.cc#L28)、[Kafka 配置](../server/conf/chat-hunan-im.yaml#L42) | 启动 Kafka，增加直接调用或业务接入点，发送消息后用 Kafka consumer 检查 `im-messages` topic。 |

## 建议补测顺序

1. 先补直接可测但未断言的功能：`/test`、用户退出清理、重复消息去重。
2. 再补协议存在但缺入口或缺实现的功能：查找用户、删除好友、点对点文件发送、群成员拉取。
3. 最后补需要设计决策或外部环境的功能：群文本/退出、故障转移、真实邮件、Kafka 接入。
