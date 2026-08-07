# P01-M02 — Synchronization and Worker Threads

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose mutex, event, wait, wake, thread start/join, and thread identity through opaque handles.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/Platform/Core/Sync.cpp`, `Sources/src/Platform/Windows/Sync.cpp`, `Sources/src/Platform/Posix/Sync.cpp`, `Sources/src/Misc/Thread.h`, `Sources/src/Misc/Thread.cpp`, `tools/zig/platform_sync_test.cpp`, `build.zig`.

- [x] Test timeout, wake, auto-reset event consumption, mutex lock/unlock, and zero-live-handle reporting through the shared runtime ABI.
- [x] Implement generational, type-validated synchronization-handle registries with stale/double-destroy rejection, slot-reuse protection, and runtime-teardown invalidation.
- [ ] Add no-throw callback trampolines; thread conversion and callback hardening remain open.
- [ ] Convert `Misc::Thread` without changing its public engine behavior.
- [x] Run 10,000 wake/wait cycles and 100 create/join cycles natively.
- [x] Verify the runtime lifecycle test reports zero live synchronization handles after event and mutex destruction.
- [x] Commit checkpoint: `platform: own synchronization and workers`.

**Evidence:** Commit `129719962` hardens the shared runtime with typed generational event/mutex handles and expands `tools/zig/platform_runtime_lifecycle_test.cpp` to cover stale handles, double destruction, cross-type rejection, slot reuse, teardown draining, and post-teardown invalidation. Windows `zig build test-platform-runtime -Dtarget=x86_64-windows-msvc -Dtest-mode=run --summary all` passed with 5/5 build steps and no remaining lifecycle-test process. Commit `70a8d0a21` extends `tools/zig/platform_sync_test.cpp`; `zig build test-platform-sync -Dtarget=x86_64-windows-msvc -Dtest-mode=run --summary all` completed 10,000 wake/wait handoffs and 100 start/stop/join cycles. Thread facade conversion and no-throw callback trampolines remain open.
