# P01-M04 — bisync parameter builder

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Assemble one correct parameter object, including the three defaults the rc API gets wrong or ignores.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test asserting the emitted JSON contains every required key with the required value, for both the pairing and the steady-state call.
- [ ] Implement `bisyncParams(allocator, ctx: SyncContext) !std.json.Value` emitting `path1` (the short link), `path2` (the named remote), `workdir` (under `stateRoot`), `filtersFile`, `conflictResolve: "newer"`, and `_async: true`.
- [ ] **Emit two different trash directories.** `backupDir1` is a local path under the profile; `backupDir2` is a path on the remote. rclone requires each backup directory to sit on its own side's filesystem, and a local path passed as `backupDir2` against a remote Path2 fails the run with `parameter to --backup-dir has to be on the same remote as destination` (verified). Place `backupDir2` outside the synced prefix and confirm the P01-M03 filters exclude it.
- [ ] **Always emit `maxDelete: 50`.** The rc path builds a zero-valued `Options{}` and assigns only when the caller passes the key, so omitting it makes any single delete abort the run — unlike the CLI, which defaults to 50.
- [ ] **Emit `resyncMode: "newer"` on every pairing call.** `conflictResolve` is ignored during a resync, which defaults to Path1 winning and renames nothing: pairing a machine whose local save is older silently destroyed the newer cloud copy in testing, with no conflict file and no trash entry. Add `resync_preserves_newer_side` to the test.
- [ ] **Never emit `force`.** It disables the excess-deletes guard along with the all-changed guard, and the sentinel already covers the latter.
- [ ] Emit `resync: true` only when the pairing state records that this profile has never paired; add `assertNoResyncWhenPaired`.
- [ ] Commit checkpoint: `cloudsync: build bisync parameters`.

**Evidence:** Unit tests assert distinct `backupDir1`/`backupDir2` filesystems, `maxDelete` present, `resyncMode` present on pairing, `force` absent, and `resync` only on first pairing.
