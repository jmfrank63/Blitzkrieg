# Phase 05 — Networking and System Integration

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Port socket lifecycle, network workers, dialogs, external actions, and crash/debug hooks without altering protocols.

**Architecture:** A fixed-width socket boundary maps WinSock/POSIX behavior. Existing network code consumes normalized errors and clock/sync services; game call sites use the system facade.

**Tech Stack:** WinSock2, POSIX sockets, C++17 workers, SDL system integration.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P05-M01 | P04-M06 | socket value/error contract | socket unit tests |
| P05-M02 | M01 | WinSock/POSIX implementations | loopback test |
| P05-M03 | M02 | network worker/timing migration | network stress |
| P05-M04 | P01-M05 | game/system call sites | injected service tests |
| P05-M05 | M03, M04 | native network/system gate | phase acceptance |

Phase exit: `test-platform-network` and `test-platform-system` on native Windows/Linux; macOS compile.
