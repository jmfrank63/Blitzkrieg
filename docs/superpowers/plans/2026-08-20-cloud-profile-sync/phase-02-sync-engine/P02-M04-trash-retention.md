# P02-M04 — two-sided trash retention

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Keep both safety nets bounded without ever pruning what protects a player.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing test over synthetic local and remote trash trees with mixed ages.
- [ ] **Two trashes, one per side.** `backupDir1` is local at `profiles/<name>/.cloudsync-trash/`; `backupDir2` is on the remote, outside the synced prefix. rclone rejects a backup directory that is not on its own side's filesystem — a local path given as `backupDir2` against a remote Path2 fails with `parameter to --backup-dir has to be on the same remote as destination`.
- [ ] Relative paths are preserved on both sides: a deleted `saves/m2.sav` lands at `<trash>/saves/m2.sav`.
- [ ] Implement `pruneTrash(dir, opts: .{ max_age_days, min_keep })` for the local side, and `pruneRemoteTrash(client, ctx, opts)` using `operations/list` plus `operations/deletefile` for the remote. Both retain at least `min_keep` most-recent entries regardless of age.
- [ ] Never prune during a sync. Run after a clean finish only, so a failed run cannot delete the files it may have just displaced.
- [ ] Confirm both trash locations are excluded from the sync filter set, and that the remote trash sits outside the synced prefix — a backup directory inside the synced path that is not filtered is an rclone error, not merely wasteful.
- [ ] Commit checkpoint: `cloudsync: prune the local and remote trashes`.

**Evidence:** Integration output shows a delete recoverable from each side's trash and age-based pruning honouring `min_keep` on both.
