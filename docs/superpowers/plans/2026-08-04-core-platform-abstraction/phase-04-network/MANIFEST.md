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
