# P04-M03 — Readiness, Broadcast, and Address Semantics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Port readiness waiting, LAN broadcast, address parsing, and timeout behavior.

**Dependencies:** P04-M02.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Socket.cpp`, `Sources/src/Platform/Windows/Socket.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `Sources/src/Net/NetA4.cpp`, `Sources/src/Net/NetServerInfo.cpp`, `tools/zig/platform_network_test.cpp`, `build.zig`.

- [x] Test zero/finite timeout, readable readiness, broadcast configuration, dotted IPv4, invalid host, and deterministic address formatting.
- [x] Implement select privately and return normalized readable/timeout/would-block behavior through the platform facade.
- [ ] Preserve LAN discovery packet frequency and target ports.
- [ ] Use simulated clocks for timeout tests and real loopback for readiness.
- [ ] Run native Windows/Linux network tests and compile macOS.
- [x] Commit: `net: port readiness and LAN addressing`

**Evidence:** Windows `zig build test-platform-network -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed with zero/finite timeout, readable loopback, broadcast option, dotted IPv4, invalid-host rejection, and nonblocking would-block checks. NetLowest’s byte-identical UDP fixture also passes. Linux/macOS gates remain open; timeout checks currently use bounded real loopback waits rather than simulated clocks.
