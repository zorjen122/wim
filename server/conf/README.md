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
- `gate.yaml`: gate HTTP service config.
- `state-single.yaml`: one Gateway and one Message topology.
- `state-multi.yaml`: two Gateway and two Message topology.
- `test-client.yaml`: TCP test client config.
- `public-test.yaml`: public DAO unit-test config.
- `verify.json`: verify service config. Secrets are intentionally blank and
  should be supplied with environment variables.

Runtime scripts default to this directory. Override paths with environment
variables such as `WIM_STATE_CONFIG`, `WIM_GATE_CONFIG`, `WIM_CHAT_CONFIGS`, or
`WIM_GATEWAY_CONFIGS` when a test needs a custom topology.

Development may use `transportSecurity.mode: insecure`. A config with
`environment: production` is rejected unless `mode: mtls`; the Gateway
certificate peer identity must match its registered `gateway_id`.

For `verify.json`, use
`WIM_VERIFY_EMAIL_USER`, `WIM_VERIFY_EMAIL_PASS`, `WIM_VERIFY_REDIS_PASSWORD`,
or `WIM_VERIFY_MYSQL_PASSWORD` to inject private values at runtime.
