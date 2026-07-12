# Server Configs

This directory is the canonical location for YAML configs used by local server
nodes and test clients.

Naming follows the owning node or role:

- `chat-hunan-im.yaml`: primary local chat node on `8090/50055`.
- `chat-beijing-im.yaml`: secondary local chat node on `8091/50056`.
- `gate.yaml`: gate HTTP service config.
- `state-single.yaml`: state service config that routes to one chat node.
- `state-multi.yaml`: state service config that routes across two chat nodes.
- `test-client.yaml`: TCP test client config.
- `public-test.yaml`: public DAO unit-test config.
- `verify.json`: verify service config. Secrets are intentionally blank and
  should be supplied with environment variables.

Runtime scripts default to this directory. Override paths with environment
variables such as `WIM_STATE_CONFIG`, `WIM_GATE_CONFIG`, `WIM_CHAT_CONFIGS`, or
`WIM_CONFIG` when a test needs a custom config. For `verify.json`, use
`WIM_VERIFY_EMAIL_USER`, `WIM_VERIFY_EMAIL_PASS`, `WIM_VERIFY_REDIS_PASSWORD`,
or `WIM_VERIFY_MYSQL_PASSWORD` to inject private values at runtime.
