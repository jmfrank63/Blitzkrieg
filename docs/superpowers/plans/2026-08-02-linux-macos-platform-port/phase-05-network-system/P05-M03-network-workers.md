# P05-M03 — Port Network Workers and Timing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove Win32 worker/event/time APIs from network retry, ack, login, and P2P paths.

**Dependencies:** P05-M02.

**Allowed files:** `Sources/src/Net/NetA4.h`, `Sources/src/Net/NetA4.cpp`, `Sources/src/Net/NetAcks.cpp`, `Sources/src/Net/NetLogin.cpp`, `Sources/src/Net/NetConnection.cpp`, `Sources/src/Net/NetPeer2Peer.cpp`, `tools/zig/platform_network_test.cpp`, `build.zig`.

- [ ] Add tests for retry timing, ack expiration, login timeout, stop during blocking wait, 100 worker restarts, and wrap-safe millisecond comparison.
- [ ] Replace thread/events/sleep/time calls with Phase 01 Clock/Sync and bounded socket waits.
- [ ] Ensure worker shutdown closes/wakes sockets before join and callbacks cannot run after owner destruction.
- [ ] Preserve retry counts, timeout constants, message ordering, and packet bytes.
- [ ] Run network tests three times under the packet timeout.
- [ ] Commit: `net: port network workers and timers`

**Evidence:** deterministic retry/ack timeline and clean worker counters.
