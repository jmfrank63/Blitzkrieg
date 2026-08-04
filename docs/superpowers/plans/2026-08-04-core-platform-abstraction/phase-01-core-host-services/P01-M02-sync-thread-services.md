# P01-M02 — Synchronization and Worker Threads

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose mutex, event, wait, wake, thread start/join, and thread identity through opaque handles.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/Platform/Core/Sync.cpp`, `Sources/src/Platform/Windows/Sync.cpp`, `Sources/src/Platform/Posix/Sync.cpp`, `Sources/src/Misc/Thread.h`, `Sources/src/Misc/Thread.cpp`, `tools/zig/platform_sync_test.cpp`, `build.zig`.

- [ ] Test auto/manual reset, timeout, wake-before-wait, mutex exclusion, callback return, join, stale handle, and destroy-with-waiter rejection.
- [ ] Implement generational handle registries and no-throw callback trampolines.
- [ ] Convert `Misc::Thread` without changing its public engine behavior.
- [ ] Run 10,000 wake/wait cycles and 100 create/join cycles natively.
- [ ] Verify runtime destruction reports zero live synchronization handles.
- [ ] Commit: `platform: own synchronization and workers`

**Evidence:** stress counts and zero-live-handle report.
