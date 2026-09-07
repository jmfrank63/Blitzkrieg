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

P00-M02 macOS checkpoint: `zig build test-cloudsync-daemon -Dtarget=aarch64-macos
-Dtest-mode=run` passes 12/12 with no real rclone installed — the tests inject
`explicit`, `game_dir` and `path_env` through a `Search` struct and drive stub
executables, so they never read the host's `PATH`. `test-cloudsync-rc` and
`test-streamio` still pass; Linux cross-compile succeeds. Commit `d2f7c7461`.

Rejections are a tagged union rather than a bool: `Availability{ ready: Found,
unavailable: Rejected }` where `Rejected` carries `.not_found`/`.too_old`/
`.not_executable` **plus the path and version it rejected**, so P07-M01 can show
the player what was found next to what is required.

Open for later packets, not defects here:

- The real game-directory lookup (`std.process.executableDirPath`) is compiled
  but never asserted; precedence over `PATH` is tested with an injected
  directory. Whatever exercises the shipped binary layout should close this.
- Windows discovery is compile-verified only. The stub binaries are `/bin/sh`
  scripts, so those cases return early there, and the MSVC target cannot be
  built from macOS. P08-M02 is where it becomes real.

P00-M03 macOS checkpoint: 22/22 pass both with and without a real rclone —
`BK_TEST_RCLONE` selects one, otherwise the three live cases return early, so a
machine without rclone still passes. With the pinned v1.75.0 binary the live
path spawns, answers `core/version`, rejects a wrong password on the same port,
reports its config as `<gamedir>/cloudsync/rclone.conf` (proving `RCLONE_CONFIG`
took effect rather than merely being set), and leaves no process behind:
`pgrep -fl rclone` is empty afterwards. Commit `631202691`.

Identity for reaping is all three of pid alive, start time equal, and the
process answering `core/version` with the credentials derived from the recorded
nonce; anything less returns `.refused_foreign_process` or
`.refused_unauthenticated` and leaves the process alone on a fresh port. Start
time comes from public `sysctl(KERN_PROC_PID)` on macOS, `/proc/<pid>/stat`
field 22 on Linux, `GetProcessTimes` on Windows. The test that pins this forges
a record naming **the test process itself** with `start_time + 1`: a pid-only
supervisor would kill its own runner.

Carried forward:

- **`std.crypto.random` does not exist in Zig 0.16.** Credentials use
  `io.randomSecure()` falling back to `io.random()`. Later packets wanting
  randomness should not reach for the name the packet text used.
- Windows is semantically analyzed only — the job object, `GetProcessTimes` and
  `TerminateProcess` compile for `x86_64-windows-gnu` (verified by injecting a
  type error and watching only that target reject it), but `x86_64-windows-msvc`
  cannot even be configured from macOS. Linux `/proc` parsing is likewise
  compile-only. P08 is where both become real.
- `waitReady` has no fast path for a child that exits immediately; it waits out
  the full budget and reports `.daemon_timeout` with the log tail.
- Refusals are returned as `Reap{outcome, pid}` rather than logged, since there
  is no logging façade yet and stderr fails a test step. P07-M01 surfaces them.

P00-M04 macOS checkpoint: `test-cloudsync-abi` passes 9/9 plus the C++ consumer
natively; `x86_64-linux-gnu`, `aarch64-linux-gnu` and `x86_64-windows-gnu`
compile. `bk_cloudsync_available` is backed by real discovery, returning 0 with
`{"reason":"not_found"}` on a bare PATH and 1 with the resolved path and version
when the pinned rclone is present. Commit `6e99fe111`.

The generation predicate is `my_gen != self.next_gen -> discard`, and the
ordering tests are gate-driven rather than timed: a stub resolver blocks on an
atomic flag the test opens by hand, so "the newer probe is still in flight" is
imposed rather than raced, and neither test can flake under load. Swapping in
the weaker `my_gen > published_gen` was demonstrated to fail the
older-finishes-first case, which is what makes that test worth having.

Carried forward:

- **`std.Thread.Mutex` does not exist in Zig 0.16** — it is `std.Io.Mutex`, and
  locking takes an `Io` because a contended wait is a cancellation point. A C
  ABI entry point has none, so the module keeps a comptime-initialised
  `std.Io.Threaded` purely to service futex wait/wake. Same guarantee, different
  type than the packet text assumed; treat this like the `std.crypto.random`
  finding when later packets need a lock.
- The library is deliberately **not** installed into the game layout yet —
  nothing loads it until the facade in P06-M01, which should add it to the
  staged runtime file lists.
- `discovery_status` carries `found`, `path`, `version`, `reason` but not
  `MIN_RCLONE`. P07-M01 wants to show what was found next to what is required,
  so that packet needs either a new field here or the minimum on the C++ side.
- `x86_64-macos` cannot be configured from this machine at all — `--sysroot is
  required when building SDL for non-native macOS targets`, reproducible on the
  unchanged tree with `test-streamio`. Pre-existing and unrelated to CloudSync.

Phase 00 gate: **met on macOS.** A Zig test drives `core/version` and an async
job against a live rclone, the daemon spawns, reports ready and is reaped with
no orphan, and the ABI is reachable from C++. Windows and Linux remain
compile-only until P08.

**Windows addendum (2026-08-20, during P01-M02):** the branch moved to a real
Windows 11 machine and every phase-00 suite now runs natively on
`x86_64-windows-msvc`: rc 6/6, daemon 22/22, abi 9/9 plus the C++ consumer.
The daemon's three live cases ran against a real rclone v1.75.0 (fetched per
machine, not committed) — spawn, readiness, wrong-password rejection on the
same port, `RCLONE_CONFIG` under `<gamedir>/cloudsync/`, and identity-checked
reaping through `GetProcessTimes`, with no rclone process left afterwards. The
phase-00 gate is therefore met on Windows too, ahead of P08-M02, which keeps
only the shipped-build acceptance. Two caveats stand: the stub-executable
discovery cases are `/bin/sh` scripts and return early on Windows, and Linux
remains compile-only.

