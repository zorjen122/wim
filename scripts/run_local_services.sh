#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   启动本地联调所需的 verify/state/file/chat/gate 服务。
#   若 Redis 未运行，会调用 start_redis.sh 启动临时 Redis；脚本退出时会清理本次启动的服务。
# 常用方式：
#   ./scripts/run_local_services.sh
# 多 chat 节点方式：
#   WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
#   WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
#     ./scripts/run_local_services.sh
# 前置条件：
#   先执行 ./scripts/build.sh；需要 MySQL 已可连接，通常也会先执行 ./scripts/init_mysql.sh。

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/wim}"
: "${WIM_REDIS_HOST:=127.0.0.1}"
: "${WIM_REDIS_PORT:=6380}"
: "${WIM_REDIS_PASSWORD:=root}"
: "${WIM_STATE_CONFIG:=$ROOT_DIR/server/conf/state-single.yaml}"
: "${WIM_GATE_CONFIG:=$ROOT_DIR/server/conf/gate.yaml}"
: "${WIM_CHAT_CONFIGS:=$ROOT_DIR/server/conf/chat-hunan-im.yaml}"

for exe in "$BUILD_DIR/state/state" "$BUILD_DIR/file/file" "$BUILD_DIR/chat/chat" "$BUILD_DIR/gate/gate" "$BUILD_DIR/test/imTest"; do
  if [[ ! -x "$exe" ]]; then
    echo "Missing build output: $exe"
    echo "Run ./scripts/build.sh first."
    exit 1
  fi
done

started_redis=0
redis_ping() {
  timeout 3 redis-cli -h "$WIM_REDIS_HOST" -p "$WIM_REDIS_PORT" -a "$WIM_REDIS_PASSWORD" PING >/dev/null 2>&1
}

if ! redis_ping; then
  started_redis=1
fi
"$ROOT_DIR/scripts/start_redis.sh"

pushd "$ROOT_DIR/server/verify" >/dev/null
npm ci
popd >/dev/null

pids=()
cleanup() {
  for pid in "${pids[@]:-}"; do
    kill "$pid" >/dev/null 2>&1 || true
  done
  if [[ "$started_redis" == "1" ]]; then
    timeout 3 redis-cli -h "$WIM_REDIS_HOST" -p "$WIM_REDIS_PORT" -a "$WIM_REDIS_PASSWORD" SHUTDOWN NOSAVE >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT INT TERM

start_service() {
  local name="$1"
  local cwd="$2"
  shift 2
  echo "Starting $name..."
  (cd "$cwd" && exec setsid "$@") &
  pids+=("$!")
  sleep 1
  if ! kill -0 "${pids[-1]}" >/dev/null 2>&1; then
    echo "$name failed to start"
    exit 1
  fi
}

start_service verify "$ROOT_DIR/server/verify" env \
  WIM_VERIFY_CONFIG="$ROOT_DIR/server/conf/verify.json" \
  WIM_VERIFY_SEND_EMAIL=0 \
  WIM_VERIFY_REDIS_HOST="$WIM_REDIS_HOST" \
  WIM_VERIFY_REDIS_PORT="$WIM_REDIS_PORT" \
  WIM_VERIFY_REDIS_PASSWORD="$WIM_REDIS_PASSWORD" \
  node "$ROOT_DIR/server/verify/server.js"
start_service state "$ROOT_DIR/server/state" env WIM_CONFIG="$WIM_STATE_CONFIG" "$BUILD_DIR/state/state"
start_service file "$ROOT_DIR/server/file" "$BUILD_DIR/file/file"
chat_index=1
for chat_config in $WIM_CHAT_CONFIGS; do
  start_service "chat-$chat_index" "$ROOT_DIR/server/chat" "$BUILD_DIR/chat/chat" "$chat_config" --normal --debug
  chat_index=$((chat_index + 1))
done
start_service gate "$ROOT_DIR/server/gate" env WIM_CONFIG="$WIM_GATE_CONFIG" "$BUILD_DIR/gate/gate"

echo "Local services are running. Gate: http://127.0.0.1:18080"
wait
