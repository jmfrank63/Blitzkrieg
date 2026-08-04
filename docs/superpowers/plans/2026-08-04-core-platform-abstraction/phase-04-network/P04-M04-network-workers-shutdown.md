# P04-M04 — Network Workers and Shutdown

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route network worker synchronization and cancellation through PlatformRuntime.

**Dependencies:** P04-M03.

**Allowed files:** `Sources/src/Net/NetConnection.cpp`, `Sources/src/Net/NetLogin.cpp`, `Sources/src/Net/NetPeer2Peer.cpp`, `Sources/src/Net/NetStream.cpp`, `Sources/src/Net/Streams.cpp`, `tools/zig/network_system_gate.cpp`, `build.zig`.

- [ ] Test start, idle wait, incoming packet wake, cancellation, peer disappearance, shutdown during receive, and immediate restart.
- [ ] Replace worker sleep/event/thread assumptions with platform services.
- [ ] Close sockets before joining workers and reject callbacks after module shutdown.
- [ ] Run 100 start/stop cycles and a two-peer loopback exchange.
- [ ] Verify zero live socket/thread/event handles after every cycle.
- [ ] Commit: `net: port workers and deterministic shutdown`

**Evidence:** two-peer transcript and live-handle counters.
