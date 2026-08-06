# P08-M01 — Target Source and Link Policy

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Construct only target-valid sources, libraries, resources, and tools for the playable graph.

**Dependencies:** P07-M06.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `tools/zig/platform_build_matrix_test.zig`.

- [x] Define source sets for shared, Windows, POSIX, Linux, macOS, Windows oracle, and excluded utilities.
- [x] Add graph policy tests for target-specific sources, resources, `.def` files, and native-run eligibility.
- [x] Target-guard `.def`, resources, subsystem, MSVC include/library paths, CRT, COM, ODBC, WinSock, DirectInput, D3D, and `user32`.
- [x] Ensure developer utilities and legacy renderer are not instantiated for Linux/macOS `game-all`.
- [x] Inspect target commands; Windows `game-all` compiles, while Windows-host Linux cross-build correctly stops at the host SDL symlink/sysroot boundary and is delegated to WSL/CI.
- [x] Commit: `build: enforce target-correct playable graph`

**Evidence:** `p08-m01-target-graph.md` and the `build_support`/platform matrix
tests record target-specific source and native-link policy.
