# Phase 00 — rc Transport Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Speak rclone's rc API from Zig, with hard deadlines, and own the lifetime of the daemon that serves it.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | — | rc JSON client, deadlines, async calls, job polling |
| P00-M02 | M01 | rclone binary discovery and version gate |
| P00-M03 | M02 | daemon spawn, readiness, shutdown, identity-checked reaping |
| P00-M04 | M03 | C ABI skeleton, availability export, build graph wiring |

Exit: a Zig test drives `core/version` and an `_async` job against a live rclone, survives a server that never replies, and leaves no orphan.

P00-M01 macOS checkpoint: `zig build test-cloudsync-rc -Dtarget=aarch64-macos
-Dtest-mode=run` passes 6/6 against an in-process stub server, needing neither
network nor the rclone binary; `-Dtarget=x86_64-linux-gnu -Dtest-mode=compile`
also passes, and `test-streamio` is unaffected. Commit `34aed6cb4`.

Two findings worth carrying forward:

- **Socket-level timeouts are unusable in this runtime.** With `SO_RCVTIMEO` a
  read returns `EAGAIN`, and `Io.Threaded`'s `netReadPosix` maps `.AGAIN` to
  `errnoBug`, which panics in Debug; its `.TIMEDOUT` arm is for Windows. Each
  call therefore races its blocking phase against a timer task through
  `std.Io.Select` and cancels the loser, `cancel()` blocking until that task has
  actually finished. Measured against a server that accepts and never writes:
  307 ms for a 300 ms budget, 1006 ms for 1000 ms. Later packets adding rc calls
  must use the same mechanism rather than reaching for socket options.
- **A test step fails if its binary writes anything to stderr**, even with every
  test passing — the Zig 0.16 build runner treats non-empty stderr as failure.
  Assertions, not printed diagnostics, are how a test carries its evidence here.

