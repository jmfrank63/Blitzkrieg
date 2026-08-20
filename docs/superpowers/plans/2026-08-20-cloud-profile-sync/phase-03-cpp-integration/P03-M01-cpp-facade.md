# P03-M01 — C++ facade

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give game code a small C++ surface over the C ABI, with no Zig or rclone concepts leaking through.

**Dependencies:** P02-M04.

**Allowed files:** `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`, `build.zig`, `tools/zig/cloudsync_facade_test.cpp`.

- [ ] Write the failing C++ test first, exercising the facade against the real library.
- [ ] Declare in namespace `NCloudSync`: `bool Available()`, `int Begin( const char *pszProfile )`, `EState Poll( int nHandle )`, `const char *Error( int nHandle )`, and `void Shutdown()`.
- [ ] Mirror the engine states in `enum EState { CS_IDLE, CS_STARTING, CS_PAIRING, CS_SYNCING, CS_DONE, CS_FAILED }` so the UI never switches on an integer from Zig.
- [ ] Keep the facade header free of Zig, JSON, and rclone vocabulary. Callers know about profiles and states, nothing else.
- [ ] Place the facade beside `NProfile` in `Sources/src/StreamIO/ProfilePaths.h` conceptually, but as a real translation unit — the profile helpers are header-only because they span dylibs, and this one must not be.
- [ ] Guard every entry point so a build without the CloudSync library still links and reports `Available() == false`.
- [ ] Commit checkpoint: `cloudsync: C++ facade over the sync ABI`.

**Evidence:** `zig build test-cloudsync-facade -Dtarget=aarch64-macos -Dtest-mode=run` passes, including the unavailable path with the library absent.
