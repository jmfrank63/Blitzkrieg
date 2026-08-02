# Phase 00 — Hermetic Build and Portable ABI Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Remove host-shell dependencies and establish target classification plus portable compiler/value-type headers before runtime code moves.

**Architecture:** Zig build artifacts perform shader compilation, staging, packaging, comparison, and verification. Target policy is centralized and Windows-native syntax is isolated behind fixed headers.

**Tech Stack:** Zig 0.16.0 `std.Build`, vendored zig-sdl3 shadercross builder, C++17 compile tests.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P00-M01 | none | Zig-built shader tools and shell audit | `audit-build-hermeticity` |
| P00-M02 | M01 | shell-free stage/package tools | `test-stage` |
| P00-M03 | M01 | supported target model | `test-build-support` |
| P00-M04 | M03 | compiler/calling convention/value types | `test-platform-headers` |
| P00-M05 | M02, M04 | compile frontier and evidence | three-target foundation compile |

Phase exit:

```text
zig build audit-build-hermeticity
zig build test-build-support
zig build test-platform-headers -Dtarget=x86_64-windows-msvc
zig build test-platform-headers -Dtarget=x86_64-linux-gnu
zig build test-platform-headers -Dtarget=aarch64-macos -Dtest-mode=compile
```

Expected: no build-path shell executable remains and all portable declarations compile for every supported target.
