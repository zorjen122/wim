#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   验证两个 chat 节点同时运行时的跨节点核心流程。
#   用户 A 登录 chat-1，用户 B 登录 chat-2，脚本检查 Redis 在线路由、
#   跨节点文本投递与 ACK 落库、跨节点好友申请/回复、好友拉取和消息拉取。
# 前置条件：
#   使用 server/conf/state-multi.yaml 和两个 chat config 启动服务。
#   典型启动方式见 --help 输出。
# 注意：
#   该脚本会向测试库写入临时用户、好友申请、好友关系和消息数据。

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<EOF
Usage: ./scripts/smoke_multi_chat.sh

Requires two chat services and shared dependencies to be running, for example:
  ./scripts/init_mysql.sh
  WIM_STATE_CONFIG="\$PWD/server/conf/state-multi.yaml" \\
  WIM_CHAT_CONFIGS="\$PWD/server/conf/chat-hunan-im.yaml \$PWD/server/conf/chat-beijing-im.yaml" \\
    ./scripts/run_local_services.sh

Verifies the currently supported multi-chat path:
  - user A logs in to chat node 1, user B logs in to chat node 2
  - Redis records both users on different IM machines
  - cross-node text delivery reaches the receiver and is ACKed in MySQL
  - cross-node friend apply/reply succeeds and can be pulled
  - session and user message pulls can see the delivered text
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
: "${WIM_REDIS_HOST:=127.0.0.1}"
: "${WIM_REDIS_PORT:=6380}"
: "${WIM_REDIS_PASSWORD:=root}"
: "${CHAT_A_HOST:=127.0.0.1}"
: "${CHAT_A_PORT:=8090}"
: "${CHAT_B_HOST:=127.0.0.1}"
: "${CHAT_B_PORT:=8091}"
: "${CHAT_A_MACHINE:=hunan-im}"
: "${CHAT_B_MACHINE:=beijing-im}"

stamp="$(date +%s%N)"
uid_a=$((400000 + stamp % 100000))
uid_b=$((uid_a + 1))
user_a="multi_a_${stamp}"
user_b="multi_b_${stamp}"
friend_msg="multi_friend_apply_${stamp}"
friend_reply="multi_friend_reply_${stamp}"
text_payload="multi_text_${stamp}"
result_file="$(mktemp /tmp/wim-multi-chat.XXXXXX.json)"
trap 'rm -f "$result_file"' EXIT

mysql_exec() {
  mysql --protocol=tcp -h"$MYSQL_HOST" -P"$MYSQL_PORT" \
    -u"$WIM_DB_USER" -p"$WIM_DB_PASSWORD" "$WIM_DB" "$@"
}

mysql_scalar() {
  mysql_exec -N -B -e "$1" 2>/dev/null
}

expect_count() {
  local label="$1"
  local query="$2"
  local expected="$3"
  local actual
  actual="$(mysql_scalar "$query")"
  if [[ "$actual" != "$expected" ]]; then
    echo "$label failed: expected $expected, got ${actual:-<empty>}"
    echo "query: $query"
    exit 1
  fi
  echo "$label ok"
}

timeout 3 redis-cli -h "$WIM_REDIS_HOST" -p "$WIM_REDIS_PORT" \
  -a "$WIM_REDIS_PASSWORD" DEL "im:user:$uid_a" "im:user:$uid_b" \
  >/dev/null 2>&1 || true

mysql_exec <<SQL
INSERT INTO users (uid, username, password, email, createTime) VALUES
  ($uid_a, '$user_a', '123456', '$user_a@example.com', '2026-07-11 00:00:00'),
  ($uid_b, '$user_b', '123456', '$user_b@example.com', '2026-07-11 00:00:00');

INSERT INTO userInfo (uid, name, age, sex, headImageURL) VALUES
  ($uid_a, 'MultiA', 21, 'test', '/images/multi-a.png'),
  ($uid_b, 'MultiB', 22, 'test', '/images/multi-b.png');
SQL

UID_A="$uid_a" \
UID_B="$uid_b" \
FRIEND_MSG="$friend_msg" \
FRIEND_REPLY="$friend_reply" \
TEXT_PAYLOAD="$text_payload" \
CHAT_A_HOST="$CHAT_A_HOST" \
CHAT_A_PORT="$CHAT_A_PORT" \
CHAT_B_HOST="$CHAT_B_HOST" \
CHAT_B_PORT="$CHAT_B_PORT" \
CHAT_A_MACHINE="$CHAT_A_MACHINE" \
CHAT_B_MACHINE="$CHAT_B_MACHINE" \
WIM_REDIS_HOST="$WIM_REDIS_HOST" \
WIM_REDIS_PORT="$WIM_REDIS_PORT" \
WIM_REDIS_PASSWORD="$WIM_REDIS_PASSWORD" \
RESULT_FILE="$result_file" \
python3 - <<'PY'
import json
import os
import socket
import struct
import subprocess
import time

UID_A = int(os.environ["UID_A"])
UID_B = int(os.environ["UID_B"])
FRIEND_MSG = os.environ["FRIEND_MSG"]
FRIEND_REPLY = os.environ["FRIEND_REPLY"]
TEXT_PAYLOAD = os.environ["TEXT_PAYLOAD"]
CHAT_A_HOST = os.environ["CHAT_A_HOST"]
CHAT_A_PORT = int(os.environ["CHAT_A_PORT"])
CHAT_B_HOST = os.environ["CHAT_B_HOST"]
CHAT_B_PORT = int(os.environ["CHAT_B_PORT"])
CHAT_A_MACHINE = os.environ["CHAT_A_MACHINE"]
CHAT_B_MACHINE = os.environ["CHAT_B_MACHINE"]
REDIS_HOST = os.environ["WIM_REDIS_HOST"]
REDIS_PORT = os.environ["WIM_REDIS_PORT"]
REDIS_PASSWORD = os.environ["WIM_REDIS_PASSWORD"]
RESULT_FILE = os.environ["RESULT_FILE"]

ID_PULL_FRIEND_LIST_REQ = 1001
ID_PULL_FRIEND_APPLY_LIST_REQ = 1003
ID_PULL_SESSION_MESSAGE_LIST_REQ = 1005
ID_PULL_MESSAGE_LIST_REQ = 1007
ID_LOGIN_INIT_REQ = 1013
ID_USER_QUIT_REQ = 1015
ID_NOTIFY_ADD_FRIEND_REQ = 1021
ID_REPLY_ADD_FRIEND_REQ = 1023
ID_TEXT_SEND_REQ = 1027
ID_ACK = 1033


def require(cond, message):
    if not cond:
        raise AssertionError(message)


def redis_machine(uid):
    output = subprocess.check_output(
        [
            "redis-cli",
            "-h",
            REDIS_HOST,
            "-p",
            REDIS_PORT,
            "-a",
            REDIS_PASSWORD,
            "GET",
            f"im:user:{uid}",
        ],
        stderr=subprocess.DEVNULL,
        text=True,
    ).strip()
    require(output, f"redis online info missing for uid {uid}")
    return json.loads(output)["machineId"]


class WimClient:
    def __init__(self, uid, host, port):
        self.uid = uid
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(5)
        self.pending = []

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def send_packet(self, service_id, body):
        data = json.dumps(body, separators=(",", ":")).encode()
        self.sock.sendall(struct.pack("!II", service_id, len(data)) + data)

    def recv_exact(self, size):
        chunks = []
        remaining = size
        while remaining:
            chunk = self.sock.recv(remaining)
            if not chunk:
                raise EOFError("chat connection closed")
            chunks.append(chunk)
            remaining -= len(chunk)
        return b"".join(chunks)

    def recv_packet(self, timeout=5):
        old_timeout = self.sock.gettimeout()
        self.sock.settimeout(timeout)
        try:
            header = self.recv_exact(8)
            service_id, size = struct.unpack("!II", header)
            body = self.recv_exact(size)
            payload = json.loads(body.decode()) if body else {}
            return service_id, payload
        finally:
            self.sock.settimeout(old_timeout)

    def request(self, service_id, body, expected_id=None, timeout=5):
        if expected_id is None:
            expected_id = service_id + 1
        self.send_packet(service_id, body)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            service, payload = self.recv_packet(max(0.1, deadline - time.monotonic()))
            if service == expected_id:
                return payload
            self.pending.append((service, payload))
        raise TimeoutError(f"uid {self.uid} did not receive service {expected_id}")

    def expect_async(self, service_id, predicate=lambda payload: True, timeout=5):
        for idx, (service, payload) in enumerate(self.pending):
            if service == service_id and predicate(payload):
                self.pending.pop(idx)
                return payload

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            service, payload = self.recv_packet(max(0.1, deadline - time.monotonic()))
            if service == service_id and predicate(payload):
                return payload
            self.pending.append((service, payload))
        raise TimeoutError(f"uid {self.uid} did not receive async service {service_id}")

    def login(self):
        rsp = self.request(ID_LOGIN_INIT_REQ, {"uid": self.uid, "init": False})
        require(rsp.get("error") == 0, f"login failed for {self.uid}: {rsp}")

    def ack(self, seq):
        self.send_packet(ID_ACK, {"seq": seq, "uid": self.uid})

    def quit(self):
        try:
            self.request(ID_USER_QUIT_REQ, {"uid": self.uid}, timeout=2)
        except Exception:
            pass
        self.close()


a = WimClient(UID_A, CHAT_A_HOST, CHAT_A_PORT)
b = WimClient(UID_B, CHAT_B_HOST, CHAT_B_PORT)
server_seq = 0
try:
    a.login()
    b.login()

    machine_a = redis_machine(UID_A)
    machine_b = redis_machine(UID_B)
    require(machine_a == CHAT_A_MACHINE, f"uid {UID_A} is on {machine_a}, expected {CHAT_A_MACHINE}")
    require(machine_b == CHAT_B_MACHINE, f"uid {UID_B} is on {machine_b}, expected {CHAT_B_MACHINE}")
    print("redis online route ok")

    client_seq = int(time.time_ns() % 900000000000)
    text_rsp = a.request(
        ID_TEXT_SEND_REQ,
        {
            "from": UID_A,
            "to": UID_B,
            "seq": client_seq,
            "sessionKey": 0,
            "data": TEXT_PAYLOAD,
        },
    )
    require(text_rsp.get("error") == 0, f"cross-node text response failed: {text_rsp}")

    text_push = b.expect_async(
        ID_TEXT_SEND_REQ,
        lambda payload: payload.get("from") == UID_A
        and payload.get("to") == UID_B
        and payload.get("data") == TEXT_PAYLOAD,
    )
    server_seq = int(text_push["seq"])
    b.ack(server_seq)
    print("cross-node text push ok")

    apply_rsp = a.request(
        ID_NOTIFY_ADD_FRIEND_REQ,
        {"from": UID_A, "to": UID_B, "requestMessage": FRIEND_MSG},
    )
    require(apply_rsp.get("error") == 0, f"cross-node friend apply failed: {apply_rsp}")
    b.expect_async(
        ID_NOTIFY_ADD_FRIEND_REQ,
        lambda payload: payload.get("from") == UID_A
        and payload.get("to") == UID_B
        and payload.get("requestMessage") == FRIEND_MSG,
    )
    print("cross-node friend apply push ok")

    apply_pull = b.request(ID_PULL_FRIEND_APPLY_LIST_REQ, {"uid": UID_B})
    require(apply_pull.get("error") == 0, f"pull friend apply failed: {apply_pull}")
    require(
        any(item.get("content") == FRIEND_MSG for item in apply_pull.get("applyList", [])),
        f"pulled friend applies missing {FRIEND_MSG}: {apply_pull}",
    )

    reply_rsp = b.request(
        ID_REPLY_ADD_FRIEND_REQ,
        {"from": UID_B, "to": UID_A, "accept": True, "replyMessage": FRIEND_REPLY},
    )
    require(reply_rsp.get("error") == 0, f"cross-node friend reply failed: {reply_rsp}")
    a.expect_async(
        ID_REPLY_ADD_FRIEND_REQ,
        lambda payload: payload.get("from") == UID_B
        and payload.get("to") == UID_A
        and payload.get("accept") is True,
    )
    print("cross-node friend reply push ok")

    friends_a = a.request(ID_PULL_FRIEND_LIST_REQ, {"uid": UID_A})
    friends_b = b.request(ID_PULL_FRIEND_LIST_REQ, {"uid": UID_B})
    require(friends_a.get("error") == 0, f"pull friend list A failed: {friends_a}")
    require(friends_b.get("error") == 0, f"pull friend list B failed: {friends_b}")
    require(
        any(item.get("uid") == UID_B for item in friends_a.get("friendList", [])),
        f"A friend list missing {UID_B}: {friends_a}",
    )
    require(
        any(item.get("uid") == UID_A for item in friends_b.get("friendList", [])),
        f"B friend list missing {UID_A}: {friends_b}",
    )
    print("cross-node friend pull ok")

    session_messages = b.request(
        ID_PULL_SESSION_MESSAGE_LIST_REQ,
        {"from": UID_A, "to": UID_B, "lastMsgId": 0, "limit": 20},
    )
    require(session_messages.get("error") == 0, f"pull session messages failed: {session_messages}")
    require(
        any(item.get("content") == TEXT_PAYLOAD for item in session_messages.get("messageList", [])),
        f"session messages missing {TEXT_PAYLOAD}: {session_messages}",
    )

    user_messages = b.request(
        ID_PULL_MESSAGE_LIST_REQ,
        {"uid": UID_B, "lastMsgId": 0, "limit": 20},
    )
    require(user_messages.get("error") == 0, f"pull user messages failed: {user_messages}")
    require(
        any(item.get("content") == TEXT_PAYLOAD for item in user_messages.get("messageList", [])),
        f"user messages missing {TEXT_PAYLOAD}: {user_messages}",
    )
    print("cross-node message pull ok")
finally:
    try:
        a.quit()
    finally:
        b.quit()

with open(RESULT_FILE, "w", encoding="utf-8") as fp:
    json.dump({"server_seq": server_seq}, fp)
PY

server_seq="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["server_seq"])' "$result_file")"

for _ in {1..10}; do
  text_count="$(mysql_scalar "SELECT COUNT(*) FROM messages WHERE messageId = $server_seq AND senderId = $uid_a AND receiverId = $uid_b AND sessionKey = '0' AND content = '$text_payload' AND status = 2 AND readDateTime <> '';")"
  if [[ "$text_count" == "1" ]]; then
    echo "cross-node text mysql ack ok"
    break
  fi
  sleep 1
done

if [[ "${text_count:-0}" != "1" ]]; then
  echo "cross-node text row was not persisted as DONE"
  mysql_exec -e "SELECT * FROM messages WHERE messageId = $server_seq;" || true
  exit 1
fi

expect_count \
  "cross-node friend apply accepted rows" \
  "SELECT COUNT(*) FROM friendApplys WHERE status = 1 AND content = '$friend_reply' AND ((fromUid = $uid_a AND toUid = $uid_b) OR (fromUid = $uid_b AND toUid = $uid_a));" \
  "2"

expect_count \
  "cross-node friend relation rows" \
  "SELECT COUNT(*) FROM friends WHERE (uidA = $uid_a AND uidB = $uid_b) OR (uidA = $uid_b AND uidB = $uid_a);" \
  "2"

echo "multi chat smoke ok"
