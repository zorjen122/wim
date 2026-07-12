#!/usr/bin/env bash
set -euo pipefail

# 作用：
#   验证关系类和拉取类接口的基础行为。
#   会创建临时用户，覆盖好友申请/同意、好友列表拉取、会话/用户消息拉取、
#   群创建、入群申请和入群同意，并用 MySQL 结果做校验。
# 前置条件：
#   已初始化 MySQL，并启动本地服务；默认连接 chat 127.0.0.1:8090。
# 注意：
#   该脚本会向测试库写入临时用户、好友、群和消息数据。

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<EOF
Usage: ./scripts/smoke_relationships.sh

Requires local services to be running, for example:
  ./scripts/run_local_services.sh

Verifies friend apply/reply, friend/message pull, group create, and group join/reply.
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
uid_a=$((300000 + stamp % 100000))
uid_b=$((uid_a + 1))
user_a="rel_a_${stamp}"
user_b="rel_b_${stamp}"
friend_msg="friend_apply_${stamp}"
friend_reply="friend_reply_${stamp}"
group_name="rel_group_${stamp}"
join_msg="join_group_${stamp}"
pull_payload="pull_message_${stamp}"
message_id=$((uid_a * 1000 + 7))
result_file="$(mktemp /tmp/wim-relationships.XXXXXX.json)"
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

mysql_exec <<SQL
INSERT INTO users (uid, username, password, email, createTime) VALUES
  ($uid_a, '$user_a', '123456', '$user_a@example.com', '2026-07-10 00:00:00'),
  ($uid_b, '$user_b', '123456', '$user_b@example.com', '2026-07-10 00:00:00');

INSERT INTO userInfo (uid, name, age, sex, headImageURL) VALUES
  ($uid_a, 'RelA', 21, 'test', '/images/rel-a.png'),
  ($uid_b, 'RelB', 22, 'test', '/images/rel-b.png');

INSERT INTO messages
  (messageId, senderId, receiverId, sessionKey, type, content, status, sendDateTime, readDateTime)
VALUES
  ($message_id, $uid_a, $uid_b, '0', 1, '$pull_payload', 1, '2026-07-10 00:00:00', '');
SQL

UID_A="$uid_a" \
UID_B="$uid_b" \
FRIEND_MSG="$friend_msg" \
FRIEND_REPLY="$friend_reply" \
GROUP_NAME="$group_name" \
JOIN_MSG="$join_msg" \
PULL_PAYLOAD="$pull_payload" \
CHAT_HOST="$CHAT_HOST" \
CHAT_PORT="$CHAT_PORT" \
RESULT_FILE="$result_file" \
python3 - <<'PY'
import json
import os
import socket
import struct
import sys
import time

UID_A = int(os.environ["UID_A"])
UID_B = int(os.environ["UID_B"])
FRIEND_MSG = os.environ["FRIEND_MSG"]
FRIEND_REPLY = os.environ["FRIEND_REPLY"]
GROUP_NAME = os.environ["GROUP_NAME"]
JOIN_MSG = os.environ["JOIN_MSG"]
PULL_PAYLOAD = os.environ["PULL_PAYLOAD"]
CHAT_HOST = os.environ["CHAT_HOST"]
CHAT_PORT = int(os.environ["CHAT_PORT"])
RESULT_FILE = os.environ["RESULT_FILE"]

ID_PULL_FRIEND_LIST_REQ = 1001
ID_PULL_FRIEND_APPLY_LIST_REQ = 1003
ID_PULL_SESSION_MESSAGE_LIST_REQ = 1005
ID_PULL_MESSAGE_LIST_REQ = 1007
ID_LOGIN_INIT_REQ = 1013
ID_USER_QUIT_REQ = 1015
ID_NOTIFY_ADD_FRIEND_REQ = 1021
ID_REPLY_ADD_FRIEND_REQ = 1023
ID_ACK = 1033
ID_GROUP_CREATE_REQ = 1035
ID_GROUP_NOTIFY_JOIN_REQ = 1037
ID_GROUP_REPLY_JOIN_REQ = 1039

ASYNC_IDS = {
    ID_NOTIFY_ADD_FRIEND_REQ,
    ID_REPLY_ADD_FRIEND_REQ,
    1027,  # ID_TEXT_SEND_REQ
    ID_GROUP_NOTIFY_JOIN_REQ,
    ID_GROUP_REPLY_JOIN_REQ,
}


def require(cond, message):
    if not cond:
        raise AssertionError(message)


class WimClient:
    def __init__(self, uid):
        self.uid = uid
        self.sock = socket.create_connection((CHAT_HOST, CHAT_PORT), timeout=5)
        self.sock.settimeout(5)

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

    def recv_packet(self):
        header = self.recv_exact(8)
        service_id, size = struct.unpack("!II", header)
        body = self.recv_exact(size)
        if body:
            payload = json.loads(body.decode())
        else:
            payload = {}
        return service_id, payload

    def ack_async(self, service_id, payload):
        if service_id in ASYNC_IDS and "seq" in payload:
            self.send_packet(ID_ACK, {"seq": payload["seq"], "uid": self.uid})

    def request(self, service_id, body, expected_id=None, timeout=5):
        if expected_id is None:
            expected_id = service_id + 1
        deadline = time.monotonic() + timeout
        self.send_packet(service_id, body)
        while time.monotonic() < deadline:
            service, payload = self.recv_packet()
            if service == expected_id:
                return payload
            self.ack_async(service, payload)
        raise TimeoutError(f"did not receive service {expected_id}")

    def login(self):
        rsp = self.request(ID_LOGIN_INIT_REQ, {"uid": self.uid})
        require(rsp.get("error") == 0, f"login failed for {self.uid}: {rsp}")

    def quit(self):
        try:
            self.request(ID_USER_QUIT_REQ, {"uid": self.uid}, timeout=2)
        except Exception:
            pass
        self.close()


def with_client(uid, func):
    client = WimClient(uid)
    try:
        client.login()
        return func(client)
    finally:
        client.quit()


def check_friend_notify(client):
    rsp = client.request(
        ID_NOTIFY_ADD_FRIEND_REQ,
        {"from": UID_A, "to": UID_B, "requestMessage": FRIEND_MSG},
    )
    require(rsp.get("error") == 0, f"notify add friend failed: {rsp}")


def check_friend_reply_and_pulls(client):
    apply_rsp = client.request(ID_PULL_FRIEND_APPLY_LIST_REQ, {"uid": UID_B})
    require(apply_rsp.get("error") == 0, f"pull friend apply failed: {apply_rsp}")
    require(
        any(item.get("content") == FRIEND_MSG for item in apply_rsp.get("applyList", [])),
        f"friend apply list missing message {FRIEND_MSG}: {apply_rsp}",
    )

    reply_rsp = client.request(
        ID_REPLY_ADD_FRIEND_REQ,
        {
            "from": UID_B,
            "to": UID_A,
            "accept": True,
            "replyMessage": FRIEND_REPLY,
        },
    )
    require(reply_rsp.get("error") == 0, f"reply add friend failed: {reply_rsp}")

    friends_rsp = client.request(ID_PULL_FRIEND_LIST_REQ, {"uid": UID_B})
    require(friends_rsp.get("error") == 0, f"pull friend list failed: {friends_rsp}")
    require(
        any(item.get("uid") == UID_A for item in friends_rsp.get("friendList", [])),
        f"friend list missing uid {UID_A}: {friends_rsp}",
    )

    session_rsp = client.request(
        ID_PULL_SESSION_MESSAGE_LIST_REQ,
        {"from": UID_A, "to": UID_B, "lastMsgId": 0, "limit": 10},
    )
    require(session_rsp.get("error") == 0, f"pull session messages failed: {session_rsp}")
    require(
        any(item.get("content") == PULL_PAYLOAD for item in session_rsp.get("messageList", [])),
        f"session message list missing payload {PULL_PAYLOAD}: {session_rsp}",
    )

    all_rsp = client.request(
        ID_PULL_MESSAGE_LIST_REQ,
        {"uid": UID_B, "lastMsgId": 0, "limit": 10},
    )
    require(all_rsp.get("error") == 0, f"pull all messages failed: {all_rsp}")
    require(
        any(item.get("content") == PULL_PAYLOAD for item in all_rsp.get("messageList", [])),
        f"user message list missing payload {PULL_PAYLOAD}: {all_rsp}",
    )


def create_group(client):
    rsp = client.request(ID_GROUP_CREATE_REQ, {"uid": UID_A, "groupName": GROUP_NAME})
    require(rsp.get("error") == 0, f"group create failed: {rsp}")
    gid = int(rsp.get("gid", 0))
    require(gid > 0, f"group create did not return gid: {rsp}")
    return gid


def join_group(client, gid):
    rsp = client.request(
        ID_GROUP_NOTIFY_JOIN_REQ,
        {"uid": UID_B, "gid": gid, "requestMessage": JOIN_MSG},
    )
    require(rsp.get("error") == 0, f"group join notify failed: {rsp}")


def approve_group_join(client, gid):
    rsp = client.request(
        ID_GROUP_REPLY_JOIN_REQ,
        {"gid": gid, "managerUid": UID_A, "requestorUid": UID_B, "accept": True},
    )
    require(rsp.get("error") == 0, f"group join reply failed: {rsp}")


with_client(UID_A, check_friend_notify)
print("friend notify ok")

with_client(UID_B, check_friend_reply_and_pulls)
print("friend reply and message pulls ok")

group_gid = with_client(UID_A, create_group)
print(f"group create ok: {group_gid}")

with_client(UID_B, lambda client: join_group(client, group_gid))
print("group join notify ok")

with_client(UID_A, lambda client: approve_group_join(client, group_gid))
print("group join reply ok")

with open(RESULT_FILE, "w", encoding="utf-8") as fp:
    json.dump({"group_gid": group_gid}, fp)
PY

group_gid="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["group_gid"])' "$result_file")"

expect_count \
  "friend apply accepted rows" \
  "SELECT COUNT(*) FROM friendApplys WHERE status = 1 AND ((fromUid = $uid_a AND toUid = $uid_b) OR (fromUid = $uid_b AND toUid = $uid_a));" \
  "2"

expect_count \
  "friend relation rows" \
  "SELECT COUNT(*) FROM friends WHERE (uidA = $uid_a AND uidB = $uid_b) OR (uidA = $uid_b AND uidB = $uid_a);" \
  "2"

expect_count \
  "message fixture row" \
  "SELECT COUNT(*) FROM messages WHERE messageId = $message_id AND senderId = $uid_a AND receiverId = $uid_b AND content = '$pull_payload';" \
  "1"

expect_count \
  "group row" \
  "SELECT COUNT(*) FROM groupInfo WHERE gid = $group_gid AND name = '$group_name';" \
  "1"

expect_count \
  "group member rows" \
  "SELECT COUNT(*) FROM groupMembers WHERE gid = $group_gid AND uid IN ($uid_a, $uid_b);" \
  "2"

expect_count \
  "group apply accepted row" \
  "SELECT COUNT(*) FROM groupApplys WHERE gid = $group_gid AND requestor = $uid_b AND handler = $uid_a AND status = 1;" \
  "1"

echo "relationship smoke ok"
