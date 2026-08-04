# P08-M01 — Target Source and Link Policy

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Construct only target-valid sources, libraries, resources, and tools for the playable graph.

**Dependencies:** P07-M06.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `tools/zig/platform_build_matrix_test.zig`.

- [ ] Define source sets for shared, Windows, POSIX, Linux, macOS, Windows oracle, and excluded utilities.
- [ ] Add failing graph tests for a Windows source/library/resource in Linux/macOS and a POSIX source in Windows.
- [ ] Target-guard `.def`, resources, subsystem, MSVC include/library paths, CRT, COM, ODBC, WinSock, DirectInput, D3D, and `user32`.
- [ ] Ensure developer utilities and legacy renderer are not instantiated for Linux/macOS `game-all`.
- [ ] Inspect `--verbose` commands for all triples.
- [ ] Commit: `build: enforce target-correct playable graph`

**Evidence:** sorted per-target source/library manifests.
