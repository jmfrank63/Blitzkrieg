# P04-M03 — restore with GFX-preserving merge

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Restore settings from another machine without importing its monitor layout.

**Dependencies:** P04-M02.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

This packet is Zig and ABI only. Applying a staged restore at startup is C++ and belongs to `P06-M02`; no file under `Sources/src/Game` or `Sources/src/GameTT` is touched here.

- [ ] Write the failing test with a backup whose `GFX.*` values differ from local, asserting they are not adopted in merge mode.
- [ ] **Stage the restore; do not write `config.cfg` under a running game.** The game rewrites that file from its in-memory options at shutdown (`GameMain.cpp:1182`) and from any of eight other `SerializeConfig( false, ... )` call sites across the interface code, so a restored file written mid-session is discarded before the player ever sees it.
- [ ] Implement `restore(allocator, ctx, entry_id, mode: RestoreMode) !void` with `RestoreMode = enum { merge_keep_local_gfx, full }`, defaulting to the merge. It downloads the chosen backup to `profiles/<name>/config.cfg.pending-restore`, records the mode alongside it, and **touches nothing else** — in particular not `config.cfg`.
- [ ] **Merge at apply time, not at stage time.** Store the backup as fetched and let `P06-M02` merge it against `config.cfg` as that file stands at the next startup. Merging early would freeze the local `GFX.*` values as they were when the player pressed restore, so a resolution changed later in the session would be quietly reverted.
- [ ] **Do not suppress the game's own config writes.** `config.cfg` and `config.cfg.pending-restore` are different files: a shutdown serialize is harmless because the pending file is applied first at the next startup, and it is actively useful, since it carries any settings the player changed in the meantime into the merge. Suppression would discard binds and unrelated settings for no benefit — and with nine call sites, suppressing two of them would be arbitrary as well as wrong.
- [ ] Implement `mergeConfig(allocator, local_cfg, restored_cfg) ![]u8` taking every key from the backup **except** those under `GFX.`, which are kept from local.
- [ ] Offer `full` but warn. It is survivable — a resolution absent from the local SDL mode list falls back to Auto, a disconnected monitor to display 0 — but both failures are silent, and silent is worse than refused.
- [ ] Restore is never automatic. Backups are pulled only on explicit player action; the cloud holds the history while the local machine keeps authority over its own display.
- [ ] Export `bk_cloudsync_backup_restore(entry_id, mode) i32`, pollable like any other network call. Own the whole export path.
- [ ] Commit checkpoint: `cloudsync: restore a config backup with a GFX-preserving merge`.

**Evidence:** Unit tests show a merge preserving local `GFX.Mode`, `GFX.Monitor`, and `GFX.FullScreen` while adopting every other key, a full restore adopting all, and a merge computed against a `config.cfg` that changed after staging.
