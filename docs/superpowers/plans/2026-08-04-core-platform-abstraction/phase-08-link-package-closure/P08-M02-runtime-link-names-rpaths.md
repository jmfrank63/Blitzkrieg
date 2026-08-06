# P08-M02 — PlatformRuntime Link Names and Loader Paths

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make every module resolve the one shared PlatformRuntime in build and staged layouts.

**Dependencies:** P08-M01.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `tools/zig/stage.zig`, `tools/zig/verify_runtime.zig`, `tools/zig/platform_linkage_test.zig`.

- [x] Test Windows import library/DLL, Linux SONAME and `$ORIGIN`, macOS install name and `@loader_path`, and rejection of absolute cache paths.
- [x] Link the playable graph through one dynamic PlatformRuntime; Clock and socket implementations are no longer archived into Misc.
- [x] Stage one target-correct runtime filename in each layout; stale utility binaries remain rejected by Zig staging.
- [x] Verify the staged manifest contract includes exactly one PlatformRuntime alongside Game and the modules.
- [x] Run linkage policy inspection on the produced build graph; binary inspection remains target-native in the Linux/macOS packets.
- [x] Commit: `build: link one shared platform runtime`

**Evidence:** `p08-m02-platform-runtime.md`, platform linkage tests, and the
Windows game-all compile gate.
