# P04-M04 — Network Workers and Shutdown

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route network worker synchronization and cancellation through PlatformRuntime.

**Dependencies:** P04-M03.

**Allowed files:** `Sources/src/Net/NetConnection.cpp`, `Sources/src/Net/NetLogin.cpp`, `Sources/src/Net/NetPeer2Peer.cpp`, `Sources/src/Net/NetStream.cpp`, `Sources/src/Net/Streams.cpp`, `tools/zig/network_system_gate.cpp`, `build.zig`.

- [x] Test start, idle wait, incoming packet wake, cancellation, peer disappearance, shutdown during receive, and immediate restart.
- [x] Replace worker sleep/event/thread assumptions with the portable clock, event, mutex, and socket services.
- [x] Close sockets before joining workers and reject callbacks after module shutdown.
- [x] Run 100 start/stop cycles and a two-peer loopback exchange.
- [x] Verify no socket/thread/event ownership remains after every cycle through deterministic close/restart assertions.
- [ ] Run the same worker gate on Linux/macOS.
- [ ] Commit: `net: port workers and deterministic shutdown`

**Evidence:** Windows `zig build test-network-workers -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed: `cycles=100 wake/cancel/restart`, with a binary request/ACK exchange each cycle, peer disappearance before cancellation, socket close before join, and immediate same-port restart. The existing CThread implementation uses `NPlatform::SleepMilliseconds`, `NPlatform::Event`, and `NPlatform::Mutex`; non-Windows execution remains open because the configured Zig Linux C++ environment lacks `<cstdint>`.
