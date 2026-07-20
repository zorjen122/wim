# Server Configs

This directory is the canonical location for YAML configs used by local server
nodes and test clients.

Naming follows the owning node or role:

- `chat-hunan-im.yaml`: primary Message node on gRPC `50055`; legacy TCP
  rollback port is `8190` and disabled by default.
- `chat-beijing-im.yaml`: secondary Message node on gRPC `50056`; legacy TCP
  rollback port is `8191` and disabled by default.
- `gateway-hunan.yaml`: primary Connection Gateway on `8090`.
- `gateway-beijing.yaml`: secondary Connection Gateway on `8091`.
- `gate.yaml`: Auth/API Gate HTTP、验证码和 SMTP 配置。
- `state-single.yaml`: one Gateway and one Message topology.
- `state-multi.yaml`: two Gateway and two Message topology.
- `test-client.yaml`: TCP test client config.
- `public-test.yaml`: public DAO unit-test config.

Runtime scripts default to this directory. Override paths with environment
variables such as `WIM_STATE_CONFIG`, `WIM_GATE_CONFIG`, `WIM_CHAT_CONFIGS`, or
`WIM_GATEWAY_CONFIGS` when a test needs a custom topology.

Development may use `transportSecurity.mode: insecure`. A config with
`environment: production` is rejected unless `mode: mtls`; the Gateway
certificate peer identity must match its registered `gateway_id`.

验证码由 Gate 进程直接处理。本地 `gate.yaml` 关闭邮件发送并通过响应返回验证码；
真实环境应设置 `verification.exposeCodeInResponse: false`、启用 SMTP，并用
`WIM_VERIFY_EMAIL_USER`、`WIM_VERIFY_EMAIL_PASS` 和可选的
`WIM_VERIFY_SMTP_URL` 注入邮件凭据。`WIM_VERIFY_SEND_EMAIL` 与
`WIM_VERIFY_EXPOSE_CODE` 可覆盖对应布尔配置。
