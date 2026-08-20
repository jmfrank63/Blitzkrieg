# Phase 02 — Sync Engine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Drive a full sync to completion off the main thread, turn every rclone failure into something a player can act on, and expose it through the ABI.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M04 | pairing and resync bootstrap |
| P02-M02 | M01 | worker thread and the sync state machine |
| P02-M03 | M02 | error classification and recovery outcomes |
| P02-M04 | M03 | two-sided trash retention and pruning |
| P02-M05 | M04 | sync exports wired through the C ABI |

Exit: a pair/diverge/converge cycle over two local directories passes end to end through the C ABI, with a conflict preserved, a delete recoverable, and no socket call on the calling thread.

P02-M01 Windows checkpoint: `zig build test-cloudsync-engine -Dtest-mode=run`
passes 7/7 natively on `x86_64-windows-msvc` against a live rclone v1.75.0
(`BK_TEST_RCLONE`), with no orphan process; the same step passes 7/7 with no
rclone, the three live cases returning early. Pairing an empty remote
delivered the save and sentinel up while `config.cfg` stayed home; the newer
side survived pairing in both directions with the loser in the correct
run-scoped trash (local loser in `backupDir1`, remote loser in `backupDir2`);
a remote sentinel was deferred to and delivered down; a repeat pair refused
`AlreadyPaired`; `syncOnce` converged with no `resync` key. Linux and
windows-gnu targets compile; all other suites unaffected. Commit `a13e63407`.

Carried forward from P02-M01:

- **`test-cloudsync-engine` is a new build step**, so `build.zig` was amended
  beyond the packet's allowlist — the build-graph continuation of P00-M04's
  wiring, same shape as the daemon step. Cross-compilation policy unchanged.
- **`backupDir1`/`backupDir2` are camelCase over rc**, confirmed in
  v1.75.0's `cmd/bisync/rc.go` (the lowercase forms are accepted for
  backward compatibility only). The public docs show lowercase; the source
  is the authority here.
- **`std.json.parseFromSlice` borrows the input buffer for strings by
  default** (`.alloc_if_needed`) — the pairing-state loader frees that
  buffer, so it parses with `.allocate = .alloc_always`. Any later packet
  parsing JSON it does not keep alive must do the same; the debug
  allocator's `0xaa` fill is what this looks like when missed.
- The live tests' remote is an `alias` remote created over `config/create`
  pointing at a fixture directory: Path2 stays a genuinely named remote (the
  only shape the plan allows) while assertions read "the cloud" with plain
  file operations. The daemon's `RCLONE_CONFIG` puts the created remote in
  `<gamedir>/cloudsync/rclone.conf`, which the fixture owns and deletes.
- `Engine.syncOnce` already exists as the blocking steady-state run —
  P02-M02 wraps it (and `pair`) on the worker thread rather than writing new
  sync logic. Both share `RunContext`; the fingerprint gate applies to both.
- The bisync job is polled at 250 ms under a 120 s budget; expiry returns
  the transport's `error.Timeout` while the job keeps running server-side.
  P02-M03 owns what to tell the player about that.
