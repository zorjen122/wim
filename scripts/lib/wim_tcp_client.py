import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time


ROOT_DIR = pathlib.Path(__file__).resolve().parents[2]
PROTO_DIR = ROOT_DIR / "server" / "public" / "proto"
GEN_DIR = pathlib.Path(tempfile.gettempdir()) / "wim_tcp_message_py"


def _ensure_proto_module():
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "protoc",
            f"--proto_path={PROTO_DIR}",
            f"--python_out={GEN_DIR}",
            str(PROTO_DIR / "tcp_message.proto"),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    if str(GEN_DIR) not in sys.path:
        sys.path.insert(0, str(GEN_DIR))


_ensure_proto_module()

from tcp_message_pb2 import Packet  # noqa: E402


ID_LOGIN_INIT_REQ = 1013
ID_USER_QUIT_REQ = 1015
ID_ACK = 1033


FIELD_MAP = {
    "sessionKey": "session_key",
    "lastMsgId": "last_msg_id",
    "requestMessage": "request_message",
    "replyMessage": "reply_message",
    "headImageURL": "head_image_url",
    "fileName": "file_name",
    "groupName": "group_name",
    "groupId": "group_id",
    "managerUid": "manager_uid",
    "requestorUid": "requestor_uid",
    "replyorUid": "replyor_uid",
    "applyDateTime": "apply_date_time",
    "sendDateTime": "send_date_time",
    "readDateTime": "read_date_time",
    "joinTime": "join_time",
    "text": "data",
}


def _set_scalar(packet, key, value):
    if key == "type" and isinstance(value, str):
        packet.file_type = value
        return
    field = FIELD_MAP.get(key, key)
    if field == "data" and isinstance(value, str):
        value = value.encode()
    setattr(packet, field, value)


def encode_packet(body):
    packet = Packet()
    for key, value in body.items():
        _set_scalar(packet, key, value)
    return packet.SerializeToString()


def _decode_bytes(value):
    try:
        return value.decode()
    except UnicodeDecodeError:
        return value.decode(errors="replace")


def decode_packet(data):
    packet = Packet()
    packet.ParseFromString(data)
    result = {}

    scalar_fields = [
        ("uid", "uid"),
        ("seq", "seq"),
        ("from", "from"),
        ("to", "to"),
        ("data", "data"),
        ("session_key", "sessionKey"),
        ("error", "error"),
        ("message", "message"),
        ("status", "status"),
        ("accept", "accept"),
        ("request_message", "requestMessage"),
        ("reply_message", "replyMessage"),
        ("username", "username"),
        ("name", "name"),
        ("age", "age"),
        ("sex", "sex"),
        ("head_image_url", "headImageURL"),
        ("init", "init"),
        ("file_name", "fileName"),
        ("file_type", "type"),
        ("type", "type"),
        ("last_msg_id", "lastMsgId"),
        ("limit", "limit"),
        ("gid", "gid"),
        ("group_id", "groupId"),
        ("group_name", "groupName"),
        ("manager_uid", "managerUid"),
        ("requestor_uid", "requestorUid"),
        ("replyor_uid", "replyorUid"),
        ("content", "content"),
        ("create_time", "createTime"),
        ("apply_date_time", "applyDateTime"),
        ("send_date_time", "sendDateTime"),
        ("read_date_time", "readDateTime"),
        ("join_time", "joinTime"),
        ("role", "role"),
        ("speech", "speech"),
    ]

    for proto_name, json_name in scalar_fields:
        if packet.HasField(proto_name):
            value = getattr(packet, proto_name)
            if proto_name == "data":
                value = _decode_bytes(value)
            result[json_name] = value

    result["friendList"] = [
        {
            "uid": item.uid,
            "name": item.name,
            "age": item.age,
            "sex": item.sex,
            "headImageURL": item.head_image_url,
        }
        for item in packet.friend_list
    ]
    result["applyList"] = [
        {
            "from": getattr(item, "from"),
            "to": item.to,
            "status": item.status,
            "content": item.content,
            "applyDateTime": item.apply_date_time,
            "createTime": item.create_time,
        }
        for item in packet.apply_list
    ]
    result["messageList"] = [
        {
            "messageId": item.message_id,
            "from": getattr(item, "from"),
            "to": item.to,
            "type": item.type,
            "content": item.content,
            "status": item.status,
            "sendDateTime": item.send_date_time,
            "readDateTime": item.read_date_time,
        }
        for item in packet.message_list
    ]
    result["memberList"] = [
        {
            "uid": item.uid,
            "name": item.name,
            "joinTime": item.join_time,
            "role": item.role,
            "speech": item.speech,
        }
        for item in packet.member_list
    ]

    return result


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class WimClient:
    def __init__(self, uid, host, port, timeout=5, async_ack_ids=None, auto_ack=False):
        self.uid = uid
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.pending = []
        self.async_ack_ids = set(async_ack_ids or [])
        self.auto_ack = auto_ack

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def send_packet(self, service_id, body):
        data = encode_packet(body)
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
            payload = decode_packet(body) if body else {}
            return service_id, payload
        finally:
            self.sock.settimeout(old_timeout)

    def ack(self, seq):
        self.send_packet(ID_ACK, {"seq": seq, "uid": self.uid})

    def ack_async(self, service_id, payload):
        if service_id in self.async_ack_ids and "seq" in payload:
            self.ack(payload["seq"])
            return True
        return False

    def request(self, service_id, body, expected_id=None, timeout=5, auto_ack=None):
        if expected_id is None:
            expected_id = service_id + 1
        if auto_ack is None:
            auto_ack = self.auto_ack

        self.send_packet(service_id, body)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = max(0.1, deadline - time.monotonic())
            try:
                service, payload = self.recv_packet(remaining)
            except socket.timeout:
                continue
            if service == expected_id:
                return payload
            if auto_ack and self.ack_async(service, payload):
                continue
            self.pending.append((service, payload))

        raise TimeoutError(f"uid {self.uid} did not receive service {expected_id}")

    def expect_async(self, service_id, predicate=lambda payload: True, timeout=5):
        for idx, (service, payload) in enumerate(self.pending):
            if service == service_id and predicate(payload):
                self.pending.pop(idx)
                return payload

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = max(0.1, deadline - time.monotonic())
            try:
                service, payload = self.recv_packet(remaining)
            except socket.timeout:
                continue
            if service == service_id and predicate(payload):
                return payload
            self.pending.append((service, payload))

        raise TimeoutError(f"uid {self.uid} did not receive async service {service_id}")

    def login(self, init=None):
        body = {"uid": self.uid}
        if init is not None:
            body["init"] = init
        rsp = self.request(ID_LOGIN_INIT_REQ, body)
        require(rsp.get("error") == 0, f"login failed for {self.uid}: {rsp}")
        return rsp

    def quit(self, wait_response=False):
        try:
            if wait_response:
                self.request(ID_USER_QUIT_REQ, {"uid": self.uid}, timeout=2)
            else:
                self.send_packet(ID_USER_QUIT_REQ, {"uid": self.uid})
        except Exception:
            pass
        self.close()
