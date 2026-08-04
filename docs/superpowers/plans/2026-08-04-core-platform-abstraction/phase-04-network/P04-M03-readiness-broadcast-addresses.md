# P04-M03 — Readiness, Broadcast, and Address Semantics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Port readiness waiting, LAN broadcast, address parsing, and timeout behavior.

**Dependencies:** P04-M02.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Socket.cpp`, `Sources/src/Platform/Windows/Socket.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `Sources/src/Net/NetA4.cpp`, `Sources/src/Net/NetServerInfo.cpp`, `tools/zig/platform_network_test.cpp`, `build.zig`.

- [ ] Test zero/finite timeout, readable/writable/error sets, subnet broadcast, dotted IPv4, invalid host, and deterministic address formatting.
- [ ] Implement poll/select privately and return normalized readiness flags.
- [ ] Preserve LAN discovery packet frequency and target ports.
- [ ] Use simulated clocks for timeout tests and real loopback for readiness.
- [ ] Run native Windows/Linux network tests and compile macOS.
- [ ] Commit: `net: port readiness and LAN addressing`

**Evidence:** readiness timeline and LAN discovery packet hash.
