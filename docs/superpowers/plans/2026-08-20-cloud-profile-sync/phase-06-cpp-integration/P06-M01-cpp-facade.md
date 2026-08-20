# P06-M01 — C++ facade

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give game code a small C++ surface over the ABI, with no Zig or rclone concepts leaking through.

**Dependencies:** P02-M05, P03-M04, P04-M04, and P05-M02 — the facade wraps every export those phases add, so it cannot be written before they exist.

**Allowed files:** `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`, `build.zig`, `tools/zig/cloudsync_facade_test.cpp`.

- [ ] Write the failing C++ test first, exercising the facade against the real library.
- [ ] Declare in namespace `NCloudSync`: `bool Available()`, `int Begin( const char *pszProfile )`, `EState Poll( int nHandle )`, `EOutcome Outcome( int nHandle )`, `const char *Error( int nHandle )`, `void Cancel( int nHandle )`, `void Release( int nHandle )`, and `void Shutdown()`.
- [ ] Cover the credentials, backup, and restore exports from phases 03 and 04 in the same namespace, so the phase-07 dialogs have one surface to call rather than reaching for the raw ABI. Those phases are Zig and ABI only by design; **this is the first packet permitted to add C++ for them**, which is why it depends on all three.
- [ ] Mirror the engine states and outcomes in C++ enums so the UI never switches on a bare integer from Zig, and pin their numeric values against the Zig side in the test.
- [ ] Keep the header free of Zig, JSON, and rclone vocabulary. Callers know about profiles, states, and outcomes, nothing else.
- [ ] Make it a real translation unit rather than a header-only helper: `NProfile` is header-only because it spans dylibs, and this one links against a single library instead.
- [ ] Guard every entry point so a build without the CloudSync library still links and reports `Available() == false`.
- [ ] Commit checkpoint: `cloudsync: C++ facade over the sync ABI`.

**Evidence:** `zig build test-cloudsync-facade -Dtarget=aarch64-macos -Dtest-mode=run` passes, including the unavailable path with the library absent and the enum-value pinning.
