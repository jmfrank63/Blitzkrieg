# P00-M03 — Build the Shared Platform Runtime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Export one ABI table and enforce single process-wide create/destroy ownership.

**Dependencies:** P00-M02.

**Allowed files:** `Sources/src/PlatformABI/PlatformRuntime.cpp`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/PlatformABI/PlatformRuntime.def`, `tools/zig/platform_runtime_lifecycle_test.cpp`, `build.zig`.

- [x] Write lifecycle tests for unsupported version, double create, invalid create, destroy-before-create, diagnostics, and clean recreate.
- [x] Add the `PlatformRuntime` dynamic artifact and export only `bk_platform_get_api` from the Windows `.def`.
- [x] Store lifecycle state once inside the shared library; return stable results and caller-buffer diagnostics.
- [x] Verify no SDL, socket, thread, or window is initialized in this foundation packet.
- [x] Run `zig build test-platform-runtime -Dtarget=x86_64-windows-msvc -Dtest-mode=run`; the test performs two clean lifecycle cycles in one process.
- [x] Commit: `platform: add shared runtime lifecycle`

**Evidence:** export list and two-cycle zero-live-state report.
