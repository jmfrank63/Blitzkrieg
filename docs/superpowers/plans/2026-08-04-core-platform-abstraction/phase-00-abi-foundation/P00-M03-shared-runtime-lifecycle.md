# P00-M03 — Build the Shared Platform Runtime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Export one ABI table and enforce single process-wide create/destroy ownership.

**Dependencies:** P00-M02.

**Allowed files:** `Sources/src/PlatformABI/PlatformRuntime.cpp`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/PlatformABI/PlatformRuntime.def`, `tools/zig/platform_runtime_lifecycle_test.cpp`, `build.zig`.

- [ ] Write failing tests for unsupported version, double create, call-before-create, destroy-before-create, callback lifetime, and clean recreate.
- [ ] Add the `PlatformRuntime` dynamic artifact and export only `bk_platform_get_api` plus the platform module descriptor required by staging.
- [ ] Store lifecycle state once inside the shared library; return stable results and caller-buffer diagnostics.
- [ ] Verify no SDL, socket, thread, or window is initialized in this foundation packet.
- [ ] Run `zig build test-platform-runtime -Dtest-mode=run` twice in one process.
- [ ] Commit: `platform: add shared runtime lifecycle`

**Evidence:** export list and two-cycle zero-live-state report.
