# P01-M04 — bisync parameter builder

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Assemble one correct `sync/bisync` parameter object, including the defaults the rc API gets wrong.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test asserting the emitted JSON contains every required key with the required value.
- [ ] Implement `bisyncParams(allocator, ctx: SyncContext) !std.json.Value` emitting `path1` (the short link), `path2` (the named remote), `workdir`, `filtersFile`, `conflictResolve: "newer"`, `backupDir1`, `backupDir2`, and `_async: true`.
- [ ] **Always emit `maxDelete: 50`.** The rc path builds a zero-valued `Options{}` and assigns only when the caller passes the key, so omitting it makes any single delete abort the run — unlike the CLI, which defaults to 50.
- [ ] **Never emit `force`.** It switches off the excess-deletes guard along with the all-changed guard, and the sentinel already covers the latter.
- [ ] Emit `resync: true` only when the pairing state file records that this profile has never paired; add `assertNoResyncWhenPaired` to the test.
- [ ] Keep `workdir` inside the game directory so state files sit beside the profile they describe rather than in a per-user cache.
- [ ] Commit checkpoint: `cloudsync: build bisync parameters`.

**Evidence:** Unit tests assert `maxDelete` present, `force` absent, `resync` only on first pairing, and an otherwise stable key set.
