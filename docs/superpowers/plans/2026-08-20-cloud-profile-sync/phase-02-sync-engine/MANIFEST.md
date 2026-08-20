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

P02-M02 Windows checkpoint: `zig build test-cloudsync-worker -Dtest-mode=run`
passes 3/3 natively on `x86_64-windows-msvc`, both with and without a live
rclone. Against a server that accepts and never replies, the maximum observed
`poll()` stayed under one 60 Hz frame while the run ended `.failed` on the
per-POST deadline; `begin` returned within a frame; a second `begin` refused
`Busy`; `destroy` mid-flight returned inside the deadline bound. Live: pair
then sync entirely through `begin`/`poll` with the worker spawning its own
daemon from a pre-written alias `rclone.conf`, no orphan. Cross-targets
compile; all other suites unaffected. Commit `8dc39cab9`.

Carried forward from P02-M02:

- The worker is one long-lived `io.concurrent` task on the caller's
  `Io.Threaded`, heap-pinned via `Worker.create`/`destroy`. The rc client's
  nested `Io.Select` deadline machinery works from inside a concurrent task —
  measured by the hung-transport case, on Windows included.
- `Snapshot` carries its error text inline (`error_buf`/`errorText()`), not
  as the packet's `[]const u8` — a slice into worker memory could be freed
  by the next transition, and P02-M05 marshals a value copy anyway.
- `Progress` is declared but always published null for now; the sync
  indicator packet decides what real progress means.
- The rclone binary reaches the worker only through `BinarySource`, a
  resolve-to-owned-copy callback. P02-M05's ABI wiring implements it over
  the discovery cache's lock; tests hand back fixture paths.
- `Engine` gained `cancel: ?*const std.atomic.Value(bool)`, checked between
  the bounded phases of `runBisync`, plus `error.Cancelled` in both error
  sets. `Worker.destroy` and `Worker.cancel` set it.
- Fixture lesson: a hung-server stub must accept exactly once and then
  sleep-loop on its stop flag — a looping `accept` leaves the join in
  `stop()` blocked forever. rc_test's fixture is shaped that way for the
  same reason; the first draft here re-learned it the hard way (test binary
  killed by hand after a 600 s hang).
- Worker tests pre-write `<gamedir>/cloudsync/rclone.conf` with the alias
  remote before the worker spawns the daemon — the exact arrangement the
  credentials packet produces in production, and it proves the daemon reads
  a pre-existing config rather than needing rc-side creation.

P02-M03 Windows checkpoint: `zig build test-cloudsync-engine -Dtest-mode=run`
passes 12/12 natively (17 with the live daemon), every classification fixture
a captured real v1.75.0 failure recorded in
`docs/superpowers/evidence/cloud-sync/failure-texts-v1.75-windows.md`.
Cross-targets compile; worker, plan and streamio unaffected. Commit
`39c193897`.

Carried forward from P02-M03:

- **The resync trailer is a consequence, not a cause.** Auth failures and
  unreachable-network failures both end in the identical `Bisync aborted.
  Must run --resync to recover.` (captured), so `classifyText` tests cause
  patterns first and a dedicated test pins the order. Anyone adding patterns
  must keep causes above the trailer.
- **Windows spells "name too long" differently**: bisync's canned `syntax
  error detected in your path(s)` wrapping `The filename, directory name, or
  volume label syntax is incorrect.` POSIX says `file name too long`. Both
  are patterns; P08-M01 should confirm the POSIX capture on macOS.
- `recovery()` is a total switch — a new `Outcome` without a decision fails
  to compile — and no `Recovery` arm can express a force retry, which is how
  "never auto-force the delete guard" is made structural.
- `Engine.recordError` stores classification (`lastOutcome()`) plus a
  redacted 200-line log tail; the raw log never escapes that function.
  `classifyTransport` covers errors with no log; rc-level `Unauthorized` is
  `daemon_gone` (foreign process on our port), never `auth_failed`.
- The connection-string capture shows rclone printing `user=…,pass=…` inside
  filesystem names in error lines — the concrete reason redaction is not
  optional. P02-M05's ABI error string carries `lastErrorText()`, which is
  already redacted; the worker snapshot's 512-byte copy truncates it, so the
  ABI should read the engine text, not the snapshot, for support reports.

P02-M04 Windows checkpoint: `zig build test-cloudsync-engine -Dtest-mode=run`
passes 15/15 natively, 17 with the live daemon — run-wise remote pruning over
`operations/list` + `operations/purge` with a foreign directory untouched and
a missing trash root reporting zero-of-each. Cross-targets compile; worker,
plan, streamio unaffected. Commit `4aea7f0df`.

Carried forward from P02-M04:

- Retention is run-wise and conservative by construction: only names that
  parse as run ids are candidates, `min_keep_runs` newest survive regardless
  of age, and a directory that refuses to delete counts as kept. Nothing the
  pruner did not create can be deleted by it.
- `runIdTimestamp` (public) inverts `plan.runId`'s stamp; pruning arithmetic
  is pinned against known epoch values including a leap day. Anyone changing
  the run-id format must change both and both tests.
- Pruning is opt-in via `Engine.prune: ?PruneOptions` and fires only inside
  `syncOnce` after `recordSuccess` — pairing does not prune (its run just
  created the first trash entries), and prune failures never fail the sync.
  P02-M05 decides the shipped `PruneOptions`; until then the field stays
  null and nothing prunes.
- The filter exclusion of the local trash and the sibling layout of the
  remote one are pinned by plan tests (P01-M03/M04), not re-proven here.
