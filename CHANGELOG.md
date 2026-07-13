# Changelog

This file records the verified project baseline and subsequent architectural
changes.

## 2026-07-14 - Authenticated Message Lifecycle

- Defined distinct client command and server message identifiers, plus explicit
  ACCEPTED, DELIVERED, and READ states.
- Added retry classification and preserved stable `message_id` results across
  local, offline, and cross-Chat successful sends.
- Added short-lived Gate-to-Chat authentication tokens and one-time atomic TCP
  session identity binding.
- Centralized TCP principal injection. Business handlers use canonical `uid`;
  client `from` and `manager_uid` fields cannot override the authenticated actor.
- Added receiver ownership checks for message receipts, monotonic state updates,
  explicit transport receipts, and stable first-read timestamps.
- Updated C++, Python, smoke, and performance clients for authenticated login and
  explicit receipts; removed credential-bearing logs.
- Verified the build and single-node, relationship, and two-node forwarding
  smoke paths.

## 2026-07-13 - Verified Baseline (`1c51a58`)

- Separate Gate, Verify, State, Chat, and File services with local configuration
  and startup scripts.
- HTTP verification, registration, sign-in, and Chat-node selection flow.
- Protobuf-framed Chat TCP protocol with local and cross-node gRPC text delivery.
- MySQL persistence for users, profiles, friends, groups, applications, and
  messages; Redis-backed ids, online routing, and short-window message dedup.
- Friend application/reply, group creation/join, message history pulls, file
  upload, and interactive test-client flows.
- Managed Chat dispatch thread pool, performance tooling, database initialization,
  and single-node/two-node smoke scripts.
