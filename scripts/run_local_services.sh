#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   启动本地联调所需的 state/file/message/gateway/gate 服务。
#   若 Redis 未运行，会调用 start_redis.sh 启动临时 Redis；脚本退出时会清理本次启动的服务。
# 常用方式：
#   ./scripts/run_local_services.sh
# 停止当前用户从本仓库启动的旧 WIM 服务，然后启动新服务：
#   ./scripts/run_local_services.sh --stop-existing
# G=2、N=2 方式：
#   WIM_STATE_CONFIG="$PWD/server/conf/state-multi.yaml" \
#   WIM_CHAT_CONFIGS="$PWD/server/conf/chat-hunan-im.yaml $PWD/server/conf/chat-beijing-im.yaml" \
#   WIM_GATEWAY_CONFIGS="$PWD/server/conf/gateway-hunan.yaml $PWD/server/conf/gateway-beijing.yaml" \
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
: "${WIM_GATEWAY_CONFIGS:=$ROOT_DIR/server/conf/gateway-hunan.yaml}"
: "${WIM_CHAT_LOG_LEVEL:=--debug}"

usage() {
  cat <<EOF
Usage: ./scripts/run_local_services.sh [--stop-existing]

Options:
  --stop-existing  Stop WIM services started by the current user from this
                   repository before starting the requested local topology.
                   MySQL and Redis are not stopped.
  -h, --help       Show this help message.
EOF
}

stop_existing=0
while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --stop-existing)
      stop_existing=1
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

for exe in "$BUILD_DIR/state/state" "$BUILD_DIR/file/file" "$BUILD_DIR/chat/chat" "$BUILD_DIR/gateway/gateway" "$BUILD_DIR/gate/gate" "$BUILD_DIR/test/imTest"; do
  if [[ ! -x "$exe" ]]; then
    echo "Missing build output: $exe"
    echo "Run ./scripts/build.sh first."
    exit 1
  fi
done

stop_existing_services() {
  if ! command -v pgrep >/dev/null 2>&1; then
    echo "Cannot use --stop-existing: pgrep is not installed." >&2
    return 1
  fi

  local -a service_commands=(
    "$BUILD_DIR/state/state"
    "$BUILD_DIR/file/file"
    "$BUILD_DIR/chat/chat"
    "$BUILD_DIR/gateway/gateway"
    "$BUILD_DIR/gate/gate"
  )
  local -a service_pids=()
  local -A seen=()
  local command pid

  pid_is_wim_service() {
    local target_pid="$1"
    local argument expected
    [[ -r "/proc/$target_pid/cmdline" ]] || return 1
    while IFS= read -r -d '' argument; do
      for expected in "${service_commands[@]}"; do
        if [[ "$argument" == "$expected" ]]; then
          return 0
        fi
      done
    done < "/proc/$target_pid/cmdline"
    return 1
  }

  for command in "${service_commands[@]}"; do
    while IFS= read -r pid; do
      if [[ -n "$pid" && "$pid" != "$$" && -z "${seen[$pid]:-}" ]] &&
        pid_is_wim_service "$pid"; then
        service_pids+=("$pid")
        seen["$pid"]=1
      fi
    done < <(pgrep -u "$(id -u)" -f -- "$command" || true)
  done

  if [[ "${#service_pids[@]}" -eq 0 ]]; then
    echo "No existing WIM services found for the current user."
    return 0
  fi

  echo "Stopping existing WIM services: ${service_pids[*]}"
  kill -TERM "${service_pids[@]}" 2>/dev/null || true

  local deadline=$((SECONDS + 8))
  local -a remaining=()
  while ((SECONDS < deadline)); do
    remaining=()
    for pid in "${service_pids[@]}"; do
      if kill -0 "$pid" 2>/dev/null && pid_is_wim_service "$pid"; then
        remaining+=("$pid")
      fi
    done
    if [[ "${#remaining[@]}" -eq 0 ]]; then
      echo "Existing WIM services stopped."
      return 0
    fi
    sleep 0.2
  done

  local -a force_stop=()
  for pid in "${remaining[@]}"; do
    if kill -0 "$pid" 2>/dev/null && pid_is_wim_service "$pid"; then
      force_stop+=("$pid")
    fi
  done
  if [[ "${#force_stop[@]}" -eq 0 ]]; then
    echo "Existing WIM services stopped."
    return 0
  fi

  echo "Forcing unresponsive WIM services to stop: ${force_stop[*]}" >&2
  kill -KILL "${force_stop[@]}" 2>/dev/null || true
  sleep 0.2
  for pid in "${force_stop[@]}"; do
    if kill -0 "$pid" 2>/dev/null && pid_is_wim_service "$pid"; then
      echo "Unable to stop WIM service process $pid." >&2
      return 1
    fi
  done
  echo "Existing WIM services stopped."
}

if [[ "$stop_existing" == "1" ]]; then
  stop_existing_services
fi

started_redis=0
redis_ping() {
  timeout 3 redis-cli -h "$WIM_REDIS_HOST" -p "$WIM_REDIS_PORT" -a "$WIM_REDIS_PASSWORD" PING >/dev/null 2>&1
}

if ! redis_ping; then
  started_redis=1
fi
"$ROOT_DIR/scripts/start_redis.sh"

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

start_service state "$ROOT_DIR/server/state" env WIM_CONFIG="$WIM_STATE_CONFIG" "$BUILD_DIR/state/state"
start_service file "$ROOT_DIR/server/file" "$BUILD_DIR/file/file"
chat_index=1
for chat_config in $WIM_CHAT_CONFIGS; do
  start_service "chat-$chat_index" "$ROOT_DIR/server/chat" "$BUILD_DIR/chat/chat" "$chat_config" --normal "$WIM_CHAT_LOG_LEVEL"
  chat_index=$((chat_index + 1))
done
gateway_index=1
for gateway_config in $WIM_GATEWAY_CONFIGS; do
  start_service "gateway-$gateway_index" "$ROOT_DIR/server/gateway" env WIM_CONFIG="$gateway_config" "$BUILD_DIR/gateway/gateway"
  gateway_index=$((gateway_index + 1))
done
start_service gate "$ROOT_DIR/server/gate" env WIM_CONFIG="$WIM_GATE_CONFIG" "$BUILD_DIR/gate/gate"

echo "Local services are running. Gate: http://127.0.0.1:18080"
wait
