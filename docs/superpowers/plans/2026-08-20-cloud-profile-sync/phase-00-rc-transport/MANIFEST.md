# Phase 00 — rc Transport Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Speak rclone's rc API from Zig and own the lifetime of the daemon that serves it.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | — | rc JSON client, sync and async calls, job polling |
| P00-M02 | M01 | rclone binary discovery and version gate |
| P00-M03 | M02 | daemon spawn, readiness, shutdown, orphan reaping |
| P00-M04 | M03 | C ABI exports and build graph wiring |

Exit: a Zig test spawns rclone, drives `core/version` and an `_async` job to completion, and leaves no orphan process.
