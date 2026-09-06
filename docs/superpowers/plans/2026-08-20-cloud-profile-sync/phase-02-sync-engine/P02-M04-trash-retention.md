# P02-M04 — two-sided trash retention

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Keep both safety nets bounded without ever pruning what protects a player.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing test over synthetic local and remote trash trees with mixed ages.
- [ ] **Two trashes, one per side, one directory per run.** `backupDir1` is local at `profiles/<name>/.cloudsync-trash/<run_id>/`; `backupDir2` is remote at `<remote>/trash/<profile>/<run_id>/`. rclone rejects a backup directory that is not on its own side's filesystem, and it overwrites an existing backup at the same resulting path — a shared root destroys the earlier copy of any filename that recurs, which for `quick.sav` and the autosaves is every time (verified).
- [ ] Relative paths are preserved within a run: a deleted `saves/m2.sav` lands at `<trash>/<run_id>/saves/m2.sav`.
- [ ] Implement `pruneTrash(dir, opts: .{ max_age_days, min_keep_runs })` for the local side and `pruneRemoteTrash(client, ctx, opts)` for the remote, both operating on whole run directories rather than individual files. Retain at least `min_keep_runs` most-recent runs regardless of age, so a burst of syncs cannot age out every recovery copy at once.
- [ ] Never prune during a sync. Run after a clean finish only, so a failed run cannot delete the files it may have just displaced.
- [ ] Confirm the local trash is excluded by the filter set and that the remote trash is a **sibling** of the synced prefix (`<remote>/trash/...`, not `<remote>/profiles/<name>/trash/...`). A backup directory inside the synced path that is not filtered is an rclone error, not merely wasteful.
- [ ] Commit checkpoint: `cloudsync: prune the local and remote trashes`.

**Evidence:** Integration output shows a delete recoverable from each side's trash, two versions of the same filename surviving across separate runs, and run-level pruning honouring `min_keep_runs` on both sides.
