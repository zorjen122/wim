#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   面向本地单 chat 节点的端到端 smoke 测试。
#   覆盖验证码、注册、登录、state 路由、文本消息落库、文件上传和在线直发消息。
# 前置条件：
#   已执行 ./scripts/build.sh 和 ./scripts/init_mysql.sh，
#   并通过 ./scripts/run_local_services.sh 启动本地服务。
# 注意：
#   该脚本会使用测试用户 1001/1002，并创建临时上传文件和临时 FIFO。

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/wim}"

: "${GATE_URL:=http://127.0.0.1:18080}"
: "${MYSQL_HOST:=127.0.0.1}"
: "${MYSQL_PORT:=3306}"
: "${WIM_DB:=chatServ}"
: "${WIM_DB_USER:=zorjen}"
: "${WIM_DB_PASSWORD:=root}"
: "${WIM_REDIS_HOST:=127.0.0.1}"
: "${WIM_REDIS_PORT:=6380}"
: "${WIM_REDIS_PASSWORD:=root}"

timeout 3 redis-cli -h "$WIM_REDIS_HOST" -p "$WIM_REDIS_PORT" -a "$WIM_REDIS_PASSWORD" \
  DEL im:user:1001 im:user:1002 >/dev/null 2>&1 || true

stamp="$(date +%s)"
signup_user="smoke_user_$stamp"
signup_email="smoke_$stamp@example.com"
text_payload="hello_from_smoke_$stamp"
upload_file="wim_upload_smoke_$stamp.txt"
upload_src="$ROOT_DIR/server/test/$upload_file"
upload_dst="$ROOT_DIR/server/file/1001/$upload_file"
receiver_fifo="/tmp/wim-smoke-receiver-$stamp.fifo"
receiver_log="/tmp/wim-smoke-receiver-$stamp.log"
receiver_pid=""
cleanup_files() {
  rm -f "$upload_src" "$upload_dst"
  if [[ -n "${receiver_pid:-}" ]]; then
    kill "$receiver_pid" >/dev/null 2>&1 || true
  fi
  rm -f "$receiver_fifo" "$receiver_log"
}
trap cleanup_files EXIT

require_error_zero() {
  local label="$1"
  local body="$2"
  if ! grep -q '"error" : 0' <<<"$body"; then
    echo "$label failed:"
    echo "$body"
    exit 1
  fi
  echo "$label ok"
}

verify_rsp="$(curl --max-time 10 -sS -H 'Content-Type: application/json' \
  -d "{\"email\":\"$signup_email\"}" \
  "$GATE_URL/post-verifycode")"
require_error_zero verify "$verify_rsp"

signup_rsp="$(curl --max-time 10 -sS -H 'Content-Type: application/json' \
  -d "{\"username\":\"$signup_user\",\"password\":\"123456\",\"email\":\"$signup_email\"}" \
  "$GATE_URL/post-signUp")"
require_error_zero signup "$signup_rsp"

signin_rsp="$(curl --max-time 10 -sS -H 'Content-Type: application/json' \
  -d '{"username":"zorjen","password":"123456"}' \
  "$GATE_URL/post-signIn")"
require_error_zero signin "$signin_rsp"
grep -q '"ip" : "127.0.0.1"' <<<"$signin_rsp"
grep -q '"port" : 8090' <<<"$signin_rsp"
echo "state route ok"

{ printf 'textSend\n1002\n%s\n' "$text_payload"; sleep 1; printf 'q\n'; } | \
  (cd "$ROOT_DIR/server/test" && \
    WIM_CONFIG="$ROOT_DIR/server/conf/test-client.yaml" \
    timeout 15 "$BUILD_DIR/test/imTest" zorjen 123456 1001 127.0.0.1 8090 >/tmp/wim-smoke-chat.log) || true

for _ in {1..10}; do
  text_count="$(mysql --protocol=tcp -h"$MYSQL_HOST" -P"$MYSQL_PORT" \
    -u"$WIM_DB_USER" -p"$WIM_DB_PASSWORD" -N -B \
    -e "SELECT COUNT(*) FROM $WIM_DB.messages WHERE content='$text_payload';" 2>/dev/null)"
  if [[ "$text_count" == "1" ]]; then
    echo "chat text ok"
    break
  fi
  sleep 1
done
if [[ "${text_count:-0}" != "1" ]]; then
  echo "chat text message was not persisted"
  exit 1
fi

printf 'file smoke %s\n' "$stamp" > "$upload_src"
{ printf 'uploadFile\n%s\n' "$upload_file"; sleep 1; printf 'q\n'; } | \
  (cd "$ROOT_DIR/server/test" && \
    WIM_CONFIG="$ROOT_DIR/server/conf/test-client.yaml" \
    timeout 15 "$BUILD_DIR/test/imTest" zorjen 123456 1001 127.0.0.1 8090 >/tmp/wim-smoke-file.log) || true

for _ in {1..10}; do
  if [[ -f "$upload_dst" ]] && cmp -s "$upload_src" "$upload_dst"; then
    echo "file upload ok"
    break
  fi
  sleep 1
done
if [[ ! -f "$upload_dst" ]] || ! cmp -s "$upload_src" "$upload_dst"; then
  echo "file upload did not create expected file: $upload_dst"
  exit 1
fi

mkfifo "$receiver_fifo"
(
  cd "$ROOT_DIR/server/test"
  WIM_CONFIG="$ROOT_DIR/server/conf/test-client.yaml" \
    "$BUILD_DIR/test/imTest" alice 123456 1002 127.0.0.1 8090 \
    < "$receiver_fifo" || true
) > "$receiver_log" 2>&1 &
receiver_pid="$!"
exec 3>"$receiver_fifo"

for _ in {1..10}; do
  if grep -q 'ID_LOGIN_INIT_RSP' "$receiver_log" 2>/dev/null; then
    break
  fi
  sleep 1
done
if ! grep -q 'ID_LOGIN_INIT_RSP' "$receiver_log" 2>/dev/null; then
  echo "receiver client did not log in"
  exit 1
fi

online_payload="hello_online_$stamp"
{ printf 'textSend\n1002\n%s\n' "$online_payload"; sleep 1; printf 'q\n'; } | \
  (cd "$ROOT_DIR/server/test" && \
    WIM_CONFIG="$ROOT_DIR/server/conf/test-client.yaml" \
    timeout 15 "$BUILD_DIR/test/imTest" zorjen 123456 1001 127.0.0.1 8090 >/tmp/wim-smoke-online-sender.log) || true

for _ in {1..10}; do
  if grep -q "$online_payload" "$receiver_log" 2>/dev/null; then
    echo "chat online direct ok"
    break
  fi
  sleep 1
done
if ! grep -q "$online_payload" "$receiver_log" 2>/dev/null; then
  echo "receiver did not get online text payload"
  exit 1
fi
exec 3>&-
wait "$receiver_pid" || true
receiver_pid=""

echo "smoke ok"
