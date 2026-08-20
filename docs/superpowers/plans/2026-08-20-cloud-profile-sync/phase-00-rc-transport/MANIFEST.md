# Phase 00 — rc Transport Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Speak rclone's rc API from Zig, with hard deadlines, and own the lifetime of the daemon that serves it.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | — | rc JSON client, deadlines, async calls, job polling |
| P00-M02 | M01 | rclone binary discovery and version gate |
| P00-M03 | M02 | daemon spawn, readiness, shutdown, identity-checked reaping |
| P00-M04 | M03 | C ABI skeleton, availability export, build graph wiring |

Exit: a Zig test drives `core/version` and an `_async` job against a live rclone, survives a server that never replies, and leaves no orphan.
