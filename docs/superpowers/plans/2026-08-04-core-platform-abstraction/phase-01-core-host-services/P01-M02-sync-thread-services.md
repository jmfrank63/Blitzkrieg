# P01-M02 — Synchronization and Worker Threads

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose mutex, event, wait, wake, thread start/join, and thread identity through opaque handles.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/Platform/Core/Sync.cpp`, `Sources/src/Platform/Windows/Sync.cpp`, `Sources/src/Platform/Posix/Sync.cpp`, `Sources/src/Misc/Thread.h`, `Sources/src/Misc/Thread.cpp`, `tools/zig/platform_sync_test.cpp`, `build.zig`.

- [x] Test timeout, wake, auto-reset event consumption, mutex lock/unlock, and zero-live-handle reporting through the shared runtime ABI.
- [ ] Implement generational handle registries and no-throw callback trampolines; the initial ABI packet uses opaque runtime-owned handles and the remaining stale-handle/trampoline hardening is still open.
- [ ] Convert `Misc::Thread` without changing its public engine behavior.
- [ ] Run 10,000 wake/wait cycles and 100 create/join cycles natively.
- [x] Verify the runtime lifecycle test reports zero live synchronization handles after event and mutex destruction.
- [x] Commit checkpoint: `platform: own synchronization and workers`.

**Evidence:** Windows `test-platform-runtime -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed event timeout/set/wait, mutex lock/unlock, and zero-live-handle checks; client ABI compilation and runtime test remain green.
