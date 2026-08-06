# Phase 04 — Portable Network Module

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Route Net through opaque platform sockets without changing packet bytes or multiplayer behavior.

| Packet | Depends on | Owns |
|---|---|---|
| P04-M01 | P01-M06 | socket ABI and error model |
| P04-M02 | M01 | `NetLowest` conversion |
| P04-M03 | M02 | readiness, broadcast, address conversion |
| P04-M04 | M03 | worker and shutdown lifecycle |
| P04-M05 | M04 | Net module and protocol gate |

Exit: Net includes no WinSock header, loopback/broadcast tests pass, and protocol fixture hashes are unchanged.

P04-M01 Windows checkpoint: portable socket type and loopback network contracts pass natively; ABI-owned generational socket handles remain open.
P04-M01 checkpoint: appended generational socket operations to the shared C ABI and passed the native ABI fixture, including stale-handle rejection and refcounted runtime shutdown.
P04-M02 Windows checkpoint: NetLowest now uses the portable socket facade; the byte-identical loopback UDP fixture passes, including oversize truncation and reinitialization. Linux compile/runtime remains open due the host Zig C++ header environment.
P04-M03 Windows checkpoint: readiness, broadcast configuration, dotted IPv4, invalid-host, timeout, and would-block contracts pass in the portable network fixture. Simulated-clock, LAN-frequency, Linux, and macOS coverage remain open.
P04-M04 Windows checkpoint: 100 worker start/stop cycles and two-peer binary request/ACK exchange pass with cancellation, peer disappearance, close-before-join, and immediate restart coverage. Linux/macOS execution remains open.
P04-M05 Windows checkpoint: real installed Net.dll factory gate passes with descriptor identity/version, exact factory type count, node-address creation/release, and target-guarded module/link policy. Protocol-oracle packet capture and Linux/macOS module gates remain open.
