# Phase 04 — Config Backup and Restore

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Keep config.cfg off the machine without letting it into the sync set, and make restoring it safe and reversible.

| Packet | Depends on | Owns |
|---|---|---|
| P04-M01 | P02-M05 | per-host config snapshot upload |
| P04-M02 | M01 | backup listing, retention, and listing export |
| P04-M03 | M02 | restore with a GFX-preserving merge |
| P04-M04 | M03 | pre-restore backup and undo |

Exit: a snapshot uploads per host, prunes to retention, restores without disturbing local display settings, and can be undone — all reachable from C++.

P04-M01 Windows checkpoint: `zig build test-cloudsync-backup -Dtest-mode=run`
passes 3/3 natively, offline and live — snapshot at the sanitised per-host
path, worker-driven chain proving no pull-down and `config.cfg` never
syncing. Cross-targets compile; worker, abi, streamio unaffected. Commit
`26a523fba`.

Carried forward from P04-M01:

- The snapshot name is a full run id plus `.cfg` — sortable and unique
  within a second, which "timestamp" alone is not. P04-M02's retention
  should parse it with `engine.runIdTimestamp` on the stem.
- `sanitizeHost` follows `NProfile::Sanitize` exactly except the empty
  fallback: "host", not "Player".
- The option plumbing is per-job (`JobSpec.backup_config` + `host`, mirrored
  in the ABI job document with defaults) — the worker owns no option state.
  `worker.zig` and `cloudsync.zig` were amended beyond the packet allowlist
  for that plumbing, as with P03-M04.
- `operations/copyfile` creates destination directories itself; no mkdir
  precedes a snapshot.

P04-M02 Windows checkpoint: backup suite 4/4 natively (2 offline, live tree
listing/pruning under `BK_TEST_RCLONE`), abi and worker green both ways,
cross-targets compile, no orphans. Commit `f13180239`.

Carried forward from P04-M02:

- The listing lives on the worker behind its mutex (`Worker.backup_list`),
  read entry-wise as value copies through `backupEntryJson`. It is replaced
  by the next listing and freed on destroy; nothing holds a pointer in.
- Retention runs only after a snapshot actually landed, per job, with
  `backup_keep_per_host` (default 10). `keep = 0` clamps to 1: the setting
  bounds history, never erases it.
- `bk_cloudsync_backup_entry` past-the-end returns -1, which is the counting
  contract; `worker.Outcome.backups_listed` (5) is appended and pinned.
  `.list_backups` shares the `testing` state with the probe.
- Only depth-two `.cfg` entries under `config-backups/<profile>/` are
  listed; stray files and nested directories are ignored, and an unparsable
  stem lists with timestamp 0 rather than being hidden.

P04-M03 Windows checkpoint: backup 15/15 natively, 17 with the live daemon
(staging + failed-download isolation); abi green offline and live with the
C++ consumer staging the newest listed snapshot and applying it locally;
cross-targets compile; no orphans. Commit `12e28f1be`.

Carried forward from P04-M03:

- **The active profile owns `config.cfg`** (`ResolveConfigFileName` in
  `iMainInternal.cpp`): it lives in the profile directory, not the game
  root. P04-M01's snapshot source moved accordingly (`ctx.path1`), and the
  apply step writes `profiles/<name>/config.cfg`. P06's facade must not
  assume a root-level config.
- `config.cfg` is the option system's XML (`<item>` blocks with `<KeyName>`
  children). `mergeConfig` substitutes whole item blocks textually; a
  payload with no item blocks passes through wholesale, which is also what
  makes non-XML test payloads behave predictably.
- The stage/apply protocol constants live in `backup.zig`
  (`restore_dir_name`, `active_name`, `commit_name`, `undo_dir_relative`,
  `latest_undo_name`). `LATEST_UNDO` is already rename-published at apply;
  P04-M04 consumes it.
- The undo/busy race test (packet line 27) rides on the worker's single-job
  discipline — `begin` refuses Busy while any job runs — and lands with the
  undo job itself in P04-M04, which must add it.
- `bk_cloudsync_apply_pending_restore(profile)` resolves
  `profiles/<name>` against the working directory (the game's convention);
  `bk_cloudsync_backup_restore` takes `game_dir` and builds the absolute
  profile path itself. Zig 0.16 rejects hard tabs inside multiline string
  literals — fixture XML uses spaces.

P04-M04 Windows checkpoint: backup 21/21 natively, 23 live; abi and worker
green both ways; the busy race pinned against a hung transport with no
rclone needed; cross-targets compile; no orphans. Commit `68a214e1c`.

Carried forward from P04-M04:

- Undo is a worker job (`.restore_undo`, local — no session, no daemon), so
  the worker's one-job discipline *is* the shared operation slot the design
  demanded. `bk_cloudsync_restore_undo` returns -1 busy while anything runs.
- Availability is 0/1/2/3 (nothing / cancellable / reinstatable / busy);
  the busy answer comes from the module's view of the worker, the other
  three from `backup.restoreUndoAvailability`. P07-M03's UI must name
  cancel and undo separately — the enum already forces it.
- Undo-of-undo is redo, for free, because the reinstate goes through the
  same stage protocol and the apply snapshots the pre-undo config under the
  new nonce. Tested.
- Undo snapshots are structurally exempt from trash pruning (`config/` is
  not a run id); the exemption is asserted in
  `trash pruning never touches the undo snapshots`.

**Phase 04 exit: met on Windows.** A snapshot uploads per host, prunes to
per-host retention with the newest always kept, restores through the staged
protocol without disturbing local display settings, and undoes byte-for-byte
— all reachable from C++ through the exports, with the apply and undo paths
purely local.
