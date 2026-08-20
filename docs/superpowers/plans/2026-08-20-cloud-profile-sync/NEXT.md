# Next Packet

Resume at `phase-06-cpp-integration/P06-M03-push-hooks.md`.

**Phases 00 through 05 are complete; P06-M01 and P06-M02 are done.** The
game pairs and syncs at startup against a live S3 store, measured on the
release build. Phase 00's gate is met
on macOS **and now on Windows** — see the two manifests. On 2026-08-20 the
branch moved to a real Windows 11 machine and every suite ran natively on
`x86_64-windows-msvc`, the target macOS could not configure: 58 CloudSync
tests plus the C++ consumer, with the daemon's live cases driven against a
real rclone v1.75.0 and no orphan left. The junction code in `plan.zig` made
real junctions from an unelevated shell for the first time — P01-M01's last
open item — and a file crossed them in both directions. Linux is compile-only
until P08-M03.

Four Zig 0.16 facts the packet texts got wrong, all recorded in the phase 00
manifest and worth knowing before writing more: `std.http.Client` requires an
`io` field, `std.crypto.random` and `std.Thread.Mutex` do not exist (use
`io.randomSecure()` and `std.Io.Mutex`), socket-level timeouts panic under
`Io.Threaded` so deadlines go through `std.Io.Select`, and a build test step
fails if its binary writes anything at all to stderr.

The design below is unchanged. The design at
`docs/superpowers/specs/2026-08-20-cloud-profile-sync-design.md` had its
behavioural claims measured against rclone v1.75.0 on macOS arm64.

## Resuming on another machine

Branch `feature/cloud-profile-sync`. Everything needed to continue is in the
repository; nothing lives only on the machine it was written on.

**Done so far:** all of phases 00, 01 and 02. Seven build steps green
natively on Windows — 90 CloudSync tests plus the C++ consumer's full
pair/diverge/converge cycle — with `test-streamio` unaffected. On the
machine's native platform, omit `-Dtarget`:

```
zig build test-cloudsync-rc      -Dtest-mode=run    #  6
zig build test-cloudsync-daemon  -Dtest-mode=run    # 22
zig build test-cloudsync-abi     -Dtest-mode=run    #  9 + C++ consumer; full sync cycle with BK_TEST_RCLONE
zig build test-cloudsync-plan    -Dtest-mode=run    # 35
zig build test-cloudsync-engine  -Dtest-mode=run    # 16 (5 more live with BK_TEST_RCLONE)
zig build test-cloudsync-worker  -Dtest-mode=run    #  3 (1 live with BK_TEST_RCLONE)
zig build test-cloudsync-creds   -Dtest-mode=run    #  8
zig build test-cloudsync-backend -Dtest-mode=run    #  2 (s3 live with BK_TEST_RCLONE + BK_TEST_MINIO; webdav with BK_TEST_RCLONE)
zig build test-cloudsync-backup  -Dtest-mode=run    # 21 (2 more live with BK_TEST_RCLONE)
zig build test-streamio          -Dtest-mode=run    # 32 (regression)
```

The macOS figures (P01-M01 and earlier, plan at 15/15) are in the phase
manifests; P01-M02's five new tests are platform-invariant arithmetic, so the
next macOS run should show 21/21 with no per-platform expectations.

Run `zig build` **from the repository root only** — anywhere else it aborts
with a FileNotFound panic. Toolchain is Zig 0.16.0.

**rclone is not in the repository and must be fetched per machine.** Only the
phase-00 daemon tests want a live one, and they skip cleanly without it:

```
BK_TEST_RCLONE=/path/to/rclone zig build test-cloudsync-daemon \
    -Dtarget=aarch64-macos -Dtest-mode=run
```

Use v1.75.0 or newer; `daemon.zig` gates at 1.66 because the plan depends on
`resyncMode`, on `backupDir1`/`backupDir2` as rc parameters, and on the
non-suffixed bisync listing filenames.

**Target matrix as it stands.** Run-verified: `aarch64-macos` (through
P01-M01) and `x86_64-windows-msvc` (everything, through P01-M04).
Compile-verified: `x86_64-linux-gnu`, `aarch64-linux-gnu`,
`x86_64-windows-gnu`. The SDL `--sysroot` refusal is symmetric: macOS targets
cannot be configured from Windows, just as `x86_64-macos` could not from the
arm Mac — so each OS re-verifies its own targets when the branch sits on it.
Linux becomes real at P08-M03; access to all three OSes is available, so the
matrix can be closed whenever a packet warrants it.

**Where the findings are.** Each phase `MANIFEST.md` carries a checkpoint per
completed packet: the exact commands, what passed, and anything later packets
must know. `docs/superpowers/evidence/cloud-sync/` holds measurements taken
outside the test suite. Read the phase-00 manifest before writing any Zig — it
records four APIs the packet texts assumed that do not exist in Zig 0.16
(`std.crypto.random`, `std.Thread.Mutex`, `std.net.Server`, `std.posix.symlink`),
plus two runtime traps: socket-level timeouts panic under `Io.Threaded`, and a
build test step fails if its binary writes anything at all to stderr.

**Next:** `phase-07-cloud-ui/P07-M01-credentials-dialog.md` — phase 06 is
closed; phase 07 begins with the credentials dialog.

## Corrections applied after review

The first draft of this plan would have failed in two ways that testing
confirmed, and both are now designed against rather than discovered later:

- **One trash was assigned to both sides.** rclone requires `backupDir1` on
  Path1's filesystem and `backupDir2` on Path2's; a local path given as
  `backupDir2` against a remote Path2 fails the run outright with `parameter
  to --backup-dir has to be on the same remote as destination` (measured).
  There are now two trashes, local and remote — see P01-M04 and P02-M04.
- **Pairing could destroy a save.** `conflictResolve` is ignored during a
  resync, which defaults to Path1 winning and renames nothing. Measured: a
  machine holding an older save overwrote the newer cloud copy with no
  conflict file and no trash entry. Every pairing call now carries
  `resyncMode: "newer"` — see P01-M04 and P02-M01.

### Second review pass

Six more, all reproduced or confirmed in source before being designed against:

- **Trash entries were not versioned.** rclone overwrites a backup at an
  existing path, so deleting `saves/m2.sav`, recreating it and deleting it
  again left only the second version (measured). Save filenames recur every
  time — `quick.sav`, the autosaves — so this destroyed recovery copies during
  ordinary play. Both trashes are now `<trash>/<run_id>/` (P01-M04, P02-M04).
- **Config backups sat inside the synced prefix**, at
  `<remote>/profiles/<name>/config-backups/`, so bisync would have pulled the
  whole history onto every machine. `config-backups/` and `trash/` are now
  siblings of `profiles/` (P01-M03, P04-M01).
- **The credentials struct could not express WebDAV**, and conflated rclone's
  S3 `provider` with the game's protocol choice. It is now a tagged union with
  `s3` and `webdav` arms and an `s3_provider` field (P03-M01, P07-M01).
- **A restored config was overwritten by the game itself.** `config.cfg` is
  rewritten from memory at shutdown (`GameMain.cpp:1182`) and on settings OK
  (`InterfaceOptionsSettings.cpp:344`), so a live restore never survived to
  the next launch. Restores and undos are now staged and applied at startup,
  and acceptance tests across a real restart (P04-M03, P04-M04, P06-M02,
  P07-M03, P08-M01, P08-M04).
- **Saving credentials did not define what an omitted secret meant.** Since
  the load path withholds it, omission now explicitly preserves, with a
  separate clear action (P03-M01).
- **First pairing never created the remote directories.** bisync aborts with
  `directory not found` against an empty remote (measured, even locally), which
  is every player's first sync. `operations/mkdir` now precedes pairing
  (P02-M01).

### Ninth review pass

- **Undo could race an in-flight restore download.** The in-flight guard
  covered restore against restore, but undo is the other writer of `ACTIVE`:
  a `LATEST_UNDO` from an earlier restore made undo available mid-download,
  the undo published its stage, and the finishing download then renamed
  `ACTIVE` over it with no error raised anywhere. One operation slot now
  covers both writers, availability reports busy as a third answer, and a race
  test holds a download mid-flight to prove it.
- **The UI still described undo availability as a bool.** It now names the
  action for its state — "Cancel pending restore" versus "Undo applied
  restore" — and stays disabled while the slot is busy.

### Eighth review pass

- **The generation predicate did not implement the rule it stated.**
  `my_gen > published_gen` lets generation 1 publish while generation 2 is
  still probing, since nothing has published yet — and generation 2 is
  typically the credentials save that just changed the path, so the worker
  would be served the superseded path for the length of the newer probe. The
  predicate is now `my_gen == next_gen`, with a test that inspects the cache
  while the newer refresh is still in flight.
- **Cancelling a staged restore still spoke of deleting a "pending file"**, a
  name predating the staged-directory protocol; taken literally it would
  delete the directory before `ACTIVE` and recreate the dangling-pointer state
  that has to be a hard error. Cancellation now follows the same teardown
  order, and `restore_undo_available` keys off `ACTIVE` and `LATEST_UNDO`.
- **The README contradicted packet ownership**, claiming every export-adding
  packet wires the facade while phases 03 and 04 run before the facade exists.
  It now matches EXECUTION's "once that file exists" rule.

### Seventh review pass

Each of these is a consequence of the previous round's fix, which is the usual
shape once ordering is made explicit:

- **`ACTIVE` teardown order was unspecified.** Deleting the stage directory
  before the pointer naming it would leave `ACTIVE` pointing at nothing, and
  the hard-error rule added last round would then brick startup. `ACTIVE` goes
  first; the absent-directory case is now separated from the invalid-stage
  case so only genuine corruption stops the game.
- **"Most recent" undo snapshot had no ordering** once snapshots were keyed by
  random nonce. A `LATEST_UNDO` pointer, rename-published, now defines it.
- **The cache mutex fixed safety, not staleness.** With the probe outside the
  lock, two overlapping refreshes can land out of order and the older can
  reinstate the path the player just replaced. Refreshes now carry a
  generation and publish only if none newer has started.

### Sixth review pass

- **The undo snapshot was conditional but not atomic.** "Skip if present"
  trusts whatever is present, so a crash mid-copy left a truncated
  `<nonce>.cfg` that the retry kept — reintroducing the corrupt-undo failure
  one step earlier than the nonce key had fixed it. It is now written to a
  temp file and renamed, with a crash test in the copy window.
- **Committed stages had no reliable order.** With `COMMIT` in two directories
  that marker says only "this stage is whole", and choosing the newest by
  metadata timestamp breaks under clock rollback, equal timestamps, or coarse
  resolution. An `ACTIVE` file naming the nonce, published by rename, is now
  the single selection point and a genuine total order.
- **The discovery cache had no thread-safety contract.** Credentials save
  refreshes it from the UI thread while the sync worker may be reading the
  path. It is now mutex-guarded, with no caller holding a pointer into it —
  readers copy out, the probe runs outside the lock, and a concurrent
  read/refresh test runs under the debug allocator.

### Fifth review pass

- **Apply was not crash-idempotent.** A crash between installing the new
  `config.cfg` and clearing the stage makes the next launch apply it again;
  the second run snapshotted the already-restored file, so "undo the most
  recent" recovered the restore instead of the original. Undo snapshots are
  now keyed by stage nonce and written once, with a crash test in that exact
  window.
- **Staging cleared the old stage before writing the new one**, so a failed
  download left neither, and two pollable restore jobs could interleave in one
  directory. Stages are now `.cloudsync-restore/<nonce>/`, the `COMMIT` write
  is the single publication point, the previous committed stage survives until
  a new one commits, and a second concurrent restore is refused.
- **Discovery refresh had no owner.** `P00-M04` caches discovery before
  `rclone_path` exists, and nothing required `creds_save` to invalidate it, so
  the dialog's promise to re-discover after a path change rested on nothing.
  The cache contract and `refresh_discovery` are defined in P00-M04, P03-M01
  owns invalidation, and an end-to-end test proves a saved override flips both
  `available` and `discovery_status` without a restart.

### Fourth review pass

- **Startup could not call the merge.** `mergeConfig` lives in Zig, but the
  only export was the staging download, and P06-M02 is C++ only — so applying
  a stage would have meant a second merge implementation in C++. Phase 04 now
  exports a local-only `apply_pending_restore`, P06-M01 wraps it, and P06-M02
  just decides when to call it.
- **The undo snapshot was taken a session too early.** Copying `config.cfg` at
  stage time undoes to a file that may be hours stale, discarding everything
  written since by any of nine `SerializeConfig` call sites. The snapshot moved
  inside the apply step, immediately before the merged result is installed.
- **The stage was not atomic.** A payload plus a separate mode marker is two
  writes, and a crash between them could apply merge content as a full restore.
  It is now a staged directory with payload, metadata, hash, and a `COMMIT`
  file written last; anything incomplete is deleted rather than interpreted.
- **The phase 06 manifest still listed the old dependencies** for P06-M01,
  disagreeing with the packet the coordinator reads alongside it.
- **The path picker asked for status the ABI could not supply.** P00-M04 now
  exports a structured `discovery_status` carrying the chosen path, version,
  and typed rejection reason, wrapped in the facade for the dialog.
- Corrected an overstatement: only local `GFX.*` survives the staging window.
  Every other key comes from the backup by design.

### Third review pass

- **Phase 04 had grown a C++ allowlist**, editing the facade, `GameMain.cpp`,
  and `InterfaceOptionsSettings.cpp` before the facade exists — introduced
  while fixing the restore-staging defect. Phase 04 is Zig and ABI only again;
  applying a staged restore moved to P06-M02, and P06-M01 now depends on
  P03-M04, P04-M04, and P05-M02 since it wraps their exports.
- **Suppressing the game's config writes was wrong.** `config.cfg` and the
  staged restore are different files, so a shutdown serialize was never a
  threat to the restore; suppression would only have discarded binds
  and unrelated settings, and it covered two of nine `SerializeConfig( false,
  ... )` call sites. Removed. The merge also moved to apply time, so settings
  changed after staging are folded in rather than frozen.
- **Pairing proved only that the winner was right.** It now passes
  `backupDir1`/`backupDir2` and asserts the loser lands in the correct
  run-scoped trash, in both directions. Verified that resync honours the backup
  directories: Path2-newer puts the local loser in `backupDir1`, Path1-newer
  puts the remote loser in `backupDir2`. The original catastrophic measurement
  destroyed the save only because no backup directory was passed at all.
- **The rclone path picker was promised and missing.** With bundling and
  auto-download out of scope, an override field is the only route for a player
  whose rclone is not on `PATH`; added to P07-M01, with
  `explicit_path_wins_over_path_entry` in P00-M02.
- **Authoritative documents disagreed with the packets**: the design listed
  five Cloud options against the plan's six, and the README still described a
  five-function ABI. Both reconciled, since workers are told to stop on
  exactly that.

Testing the first-pass corrections turned up a third: **the sentinel must never be seeded
on both sides.** Two independently created `.bkprofile` files differ in
modification time, and bisync then aborts the resync with `Modtime not equal
in listing` followed by `path1 and path2 are out of sync`. P01-M03 and P02-M01
check the remote before writing one.

Structural corrections in the same pass: the C ABI is now amended by whichever
packet adds an export rather than stubbed early (EXECUTION.md carries the
rule, and P02-M05, P03-M01, P03-M04, P04-M02, P04-M03, P04-M04 each own their
export path); machine-local state moved out of Path1 to `<gamedir>/cloudsync/`;
a worker thread carries every rc call because `_async` only makes the *job*
asynchronous, not the POST; `Cloud.Sync.OnSave` is declared; and the phases
were reordered so every option exists before a hook reads it.

## Still unproven

Both are Windows-first and should not be discovered late:

Both are now **settled**, and neither forces a redesign:

- ~~junction creation without administrator rights~~ — `New-Item -ItemType
  Junction` succeeds from an unelevated Windows prompt. The target must be
  absolute, which P01-M01 records.
- ~~whether rclone resolves a junction root~~ — it does not. A junction at
  `C:\bk\p0` pointing at a deep target produced the session name
  `C__bk_p0..C__bk_remote`; `bilib.FsPath` strips the `\\?\` prefix rclone
  canonicalises to and mangles the path as given. Evidence in
  `docs/superpowers/evidence/cloud-sync/junction-session-name.md`.

The short link is therefore a real mitigation on both platforms and P01-M01
stands as written. One gap remains from that probe: both sides were empty, so
data movement through a junction is unexercised — P01-M01 now carries a file
across the link, and P08-M02 confirms it in the shipped build.

## Important working-tree files

The branch is `feature/cloud-profile-sync`. The CloudSync Zig sources live in
`Sources/src/CloudSync/`; everything else is documentation. The pinned rclone
binary used for verification is not in the repository and must be fetched per
machine.
