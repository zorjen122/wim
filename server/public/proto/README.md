# Public Protocols

`server/public/proto` is the canonical home for C++ service protocols shared by
more than one module.

- `tcp_message.proto`: TLV body payload used by chat TCP clients and servers.
- `im.proto`: chat-to-chat gRPC forwarding.
- `file.proto`: chat-to-file gRPC upload/send RPC.
- `state.proto`: gate-to-state routing RPC.

The C++ build generates these files once in `server/public` as the `imProto`
target. Service targets depend on `imPublic`/`imProto` instead of generating
their own copies.

`server/verify/message.proto` remains beside the Node.js verify service because
it is loaded directly by that runtime and is also generated for the C++ gate
client.
