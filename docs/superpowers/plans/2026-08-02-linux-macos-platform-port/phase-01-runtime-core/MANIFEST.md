# Phase 01 — Portable Runtime Core

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Replace the reusable Win32 timing, synchronization, diagnostics, dynamic-library, and system-service primitives used across runtime modules.

**Architecture:** Small C++17 platform components expose SDL-free engine contracts. SDL is used privately for dynamic loading and user-facing system integration where it already supplies a portable API.

**Tech Stack:** C++17 standard clock/thread library, SDL3 load-object and system APIs, Zig C++ test executables.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P01-M01 | P00-M05 | monotonic clock and sleep | clock unit test |
| P01-M02 | P01-M01 | event/mutex/thread primitives | synchronization stress |
| P01-M03 | P01-M01 | diagnostics and assertions | debug capture test |
| P01-M04 | P01-M03 | dynamic library wrapper | symbol-load test |
| P01-M05 | P01-M04 | dialogs/process/environment facade | system service test |

Phase exit: run `zig build test-platform-core -Dtarget=x86_64-windows-msvc -Dtest-mode=run` on Windows and `zig build test-platform-core -Dtarget=x86_64-linux-gnu -Dtest-mode=run` on Linux; compile with `zig build test-platform-core -Dtarget=aarch64-macos -Dtest-mode=compile`.
