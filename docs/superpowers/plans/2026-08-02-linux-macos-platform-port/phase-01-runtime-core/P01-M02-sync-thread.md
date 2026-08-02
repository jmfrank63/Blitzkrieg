# P01-M02 — Replace Win32 Synchronization and Worker Threads

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace event handles, critical sections, and `CreateThread` in the shared worker abstraction.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/Platform/Sync.h`, `Sources/src/Platform/Sync.cpp`, `Sources/src/Misc/Win32Helper.h`, `Sources/src/Misc/Thread.h`, `Sources/src/Misc/Thread.cpp`, `tools/zig/platform_sync_test.cpp`, `build.zig`.

- [ ] Test manual-reset event set/reset/wait, zero-time query, mutex exclusion, start/stop idempotence, and 100 rapid worker start/stop cycles.
- [ ] Implement event state with `std::mutex`/`std::condition_variable` and workers with `std::thread`; all destructors join owned threads.
- [ ] Preserve `NWin32Helper::CEvent`, `CCriticalSection`, and `CCriticalSectionLock` source compatibility while moving their storage to platform types.
- [ ] Refactor `CThread` so stop signals before join and cannot close/recreate event handles.
- [ ] Run the synchronization stress three times with a 10-second total timeout.
- [ ] Commit: `platform: replace Win32 worker primitives`

**Evidence:** stress count and clean thread completion.
