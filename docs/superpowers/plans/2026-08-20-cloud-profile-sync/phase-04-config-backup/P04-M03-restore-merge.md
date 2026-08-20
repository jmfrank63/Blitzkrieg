# P04-M03 — restore with GFX-preserving merge

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Restore settings from another machine without importing its monitor layout.

**Dependencies:** P04-M02.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test with a backup whose `GFX.*` values differ from local, asserting they are not adopted in merge mode.
- [ ] Implement `restore(allocator, ctx, entry_id, mode: RestoreMode) !void` with `RestoreMode = enum { merge_keep_local_gfx, full }`, defaulting to the merge.
- [ ] Implement `mergeConfig(allocator, local_cfg, restored_cfg) ![]u8` taking every key from the backup **except** those under `GFX.`, which are kept from local.
- [ ] Offer `full` but warn. It is survivable — a resolution absent from the local SDL mode list falls back to Auto, a disconnected monitor to display 0 — but both failures are silent, and silent is worse than refused.
- [ ] Restore is never automatic. Backups are pulled only on explicit player action; the cloud holds the history while the local machine keeps authority over its own display.
- [ ] Export `bk_cloudsync_backup_restore(entry_id, mode) i32`, pollable like any other network call. Own the whole export path.
- [ ] Commit checkpoint: `cloudsync: restore a config backup with a GFX-preserving merge`.

**Evidence:** Unit tests show a merge preserving local `GFX.Mode`, `GFX.Monitor`, and `GFX.FullScreen` while adopting every other key, and a full restore adopting all.
