#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   专门验证在线文本消息投递语义。
#   通过原始 TCP 协议包登录 1001/1002，确认接收方收到 ID_TEXT_SEND_REQ，
#   ACK 后不会收到 ID_NULL，并检查 MySQL messages 已落库且状态更新为 DONE。
# 前置条件：
#   已初始化 MySQL，并启动至少一个 chat 服务；默认连接 127.0.0.1:8090。

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<EOF
Usage: ./scripts/smoke_text_delivery.sh

Requires local services and the test database to be running, for example:
  ./scripts/init_mysql.sh
  ./scripts/run_local_services.sh

Verifies online text delivery with protocol-level checks plus MySQL persistence:
  - receiver gets ID_TEXT_SEND_REQ
  - receiver ACK does not produce ID_NULL
  - messages contains the delivered text row
  - ACK marks the message as DONE
EOF
  exit 0
fi

if [[ "$#" -gt 0 ]]; then
  echo "Unknown argument: $1" >&2
  echo "Run with --help for usage." >&2
  exit 2
fi

: "${MYSQL_HOST:=127.0.0.1}"
: "${MYSQL_PORT:=3306}"
: "${WIM_DB:=chatServ}"
: "${WIM_DB_USER:=zorjen}"
: "${WIM_DB_PASSWORD:=root}"
: "${CHAT_HOST:=127.0.0.1}"
: "${CHAT_PORT:=8090}"

stamp="$(date +%s%N)"
payload="online_text_persist_${stamp}"
result_file="$(mktemp /tmp/wim-text-delivery.XXXXXX.json)"
trap 'rm -f "$result_file"' EXIT

UID_SENDER=1001 \
UID_RECEIVER=1002 \
PAYLOAD="$payload" \
CHAT_HOST="$CHAT_HOST" \
CHAT_PORT="$CHAT_PORT" \
RESULT_FILE="$result_file" \
PYTHONPATH="$ROOT_DIR/scripts/lib${PYTHONPATH:+:$PYTHONPATH}" \
python3 - <<'PY'
import json
import os
import socket
import time

from wim_tcp_client import WimClient, require

UID_SENDER = int(os.environ["UID_SENDER"])
UID_RECEIVER = int(os.environ["UID_RECEIVER"])
PAYLOAD = os.environ["PAYLOAD"]
CHAT_HOST = os.environ["CHAT_HOST"]
CHAT_PORT = int(os.environ["CHAT_PORT"])
RESULT_FILE = os.environ["RESULT_FILE"]

ID_TEXT_SEND_REQ = 1027
ID_ACK = 1033
ID_NULL = 1034


sender = WimClient(UID_SENDER, CHAT_HOST, CHAT_PORT)
receiver = WimClient(UID_RECEIVER, CHAT_HOST, CHAT_PORT)

try:
    sender.login()
    receiver.login()

    client_seq = int(time.time_ns() % 9_000_000_000 + 1_000_000_000)
    sender_rsp = sender.request(
        ID_TEXT_SEND_REQ,
        {
            "seq": client_seq,
            "from": UID_SENDER,
            "to": UID_RECEIVER,
            "data": PAYLOAD,
            "sessionKey": 0,
        },
    )
    require(sender_rsp.get("error") == 0, f"text send failed: {sender_rsp}")
    require(sender_rsp.get("seq") == client_seq, f"sender rsp seq mismatch: {sender_rsp}")

    receiver_service, receiver_push = receiver.recv_packet()
    require(
        receiver_service == ID_TEXT_SEND_REQ,
        f"receiver did not get text push: {receiver_service}, {receiver_push}",
    )
    require(receiver_push.get("data") == PAYLOAD, f"payload mismatch: {receiver_push}")
    server_seq = int(receiver_push["seq"])

    receiver.send_packet(ID_ACK, {"seq": server_seq, "uid": UID_RECEIVER})
    try:
        extra_service, extra_payload = receiver.recv_packet(timeout=0.75)
    except socket.timeout:
        extra_service = None
        extra_payload = None

    require(
        extra_service != ID_NULL,
        f"ACK unexpectedly produced ID_NULL: {extra_payload}",
    )
    require(
        extra_service is None,
        f"ACK produced unexpected packet {extra_service}: {extra_payload}",
    )

    with open(RESULT_FILE, "w", encoding="utf-8") as fp:
        json.dump({"server_seq": server_seq, "payload": PAYLOAD}, fp)
finally:
    sender.quit()
    receiver.quit()
PY

server_seq="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["server_seq"])' "$result_file")"

mysql_scalar() {
  mysql --protocol=tcp -h"$MYSQL_HOST" -P"$MYSQL_PORT" \
    -u"$WIM_DB_USER" -p"$WIM_DB_PASSWORD" "$WIM_DB" \
    -N -B -e "$1" 2>/dev/null
}

for _ in {1..10}; do
  row_count="$(mysql_scalar "SELECT COUNT(*) FROM messages WHERE messageId = $server_seq AND senderId = 1001 AND receiverId = 1002 AND sessionKey = '0' AND content = '$payload' AND status = 2 AND readDateTime <> '';")"
  if [[ "$row_count" == "1" ]]; then
    echo "online text persisted and acked ok"
    break
  fi
  sleep 1
done

if [[ "${row_count:-0}" != "1" ]]; then
  echo "online text row was not persisted as DONE"
  mysql --protocol=tcp -h"$MYSQL_HOST" -P"$MYSQL_PORT" \
    -u"$WIM_DB_USER" -p"$WIM_DB_PASSWORD" "$WIM_DB" \
    -e "SELECT * FROM messages WHERE messageId = $server_seq;" || true
  exit 1
fi

echo "text delivery smoke ok"
