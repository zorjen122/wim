# Chat TCP Perf Tests

`imPerf` provides concurrent TCP performance tests for the chat transport.

Chat authentication is mandatory. Before a run, sign each sender/receiver UID
in through Gate and create a token file with one `uid chatToken` pair per line.
Pass it using `--auth-token-file`; missing identities fail closed.

## Basic Transport

Exercises `ChatServer` and `ChatSession` with `ID_LOGIN_INIT_REQ` followed by
repeated `ID_PING_REQ` roundtrips.

```bash
./build/wim/test/imPerf \
  --mode basic \
  --endpoints 127.0.0.1:8090 \
  --connections 32 \
  --requests 1000 \
  --auth-token-file /tmp/wim-perf-tokens.txt
```

## Text Message

Exercises `ID_TEXT_SEND_REQ`. With `--receivers 0`, target users are offline
and the test measures sender request handling plus message persistence. With
receivers enabled, target users stay online and ACK pushed messages.

```bash
./build/wim/test/imPerf \
  --mode text \
  --endpoints 127.0.0.1:8090 \
  --connections 16 \
  --requests 100 \
  --receivers 16 \
  --auth-token-file /tmp/wim-perf-tokens.txt
```

For a two-node chat cluster, distribute senders and receivers across nodes:

```bash
./build/wim/test/imPerf \
  --mode text \
  --endpoints 127.0.0.1:8090,127.0.0.1:8091 \
  --receiver-endpoints 127.0.0.1:8091,127.0.0.1:8090 \
  --connections 32 \
  --requests 100 \
  --receivers 32 \
  --auth-token-file /tmp/wim-perf-tokens.txt
```

The report includes attempted requests, succeeded requests, success rate,
success throughput, attempt throughput, bandwidth, and RTT min/avg/p50/p95/p99/max.
