# P04-M03 — restore with GFX-preserving merge

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Restore settings from another machine without importing its monitor layout.

**Dependencies:** P04-M02.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`, `Sources/src/Game/GameMain.cpp`, `Sources/src/GameTT/InterfaceOptionsSettings.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Write the failing test with a backup whose `GFX.*` values differ from local, asserting they are not adopted in merge mode.
- [ ] **Stage the restore; do not write `config.cfg` under a running game.** The game writes its in-memory options back over the file at shutdown — `SerializeConfig( false, SERIALIZE_CONFIG_OPTIONS | ... )` in `GameMain.cpp:1182` — and the settings screen does the same on OK, `SerializeConfig( false, 0xffffffff )` in `InterfaceOptionsSettings.cpp:344`. A restored file written mid-session is therefore silently discarded before the player ever sees it.
- [ ] Implement `restore(allocator, ctx, entry_id, mode: RestoreMode) !void` with `RestoreMode = enum { merge_keep_local_gfx, full }`, defaulting to the merge. It writes the merged result to `profiles/<name>/config.cfg.pending-restore` and touches nothing else.
- [ ] Apply the pending file at startup, before `OptionSystem::Init()` and before the config is read — the same early window `P06-M02` already uses — by moving it over `config.cfg` and deleting the marker. Applying it there means the option system loads the restored values, so the shutdown serialize writes them back unchanged instead of fighting them.
- [ ] Suppress the options serialize for the remainder of a session in which a restore was staged, via a flag on the facade consulted at both call sites above. Otherwise the exit write races the pending file and which one wins depends on ordering.
- [ ] Tell the player plainly that a restart is required, and that settings changed after staging a restore will not survive it. Predictable and stated beats clever and surprising.
- [ ] Implement `mergeConfig(allocator, local_cfg, restored_cfg) ![]u8` taking every key from the backup **except** those under `GFX.`, which are kept from local.
- [ ] Offer `full` but warn. It is survivable — a resolution absent from the local SDL mode list falls back to Auto, a disconnected monitor to display 0 — but both failures are silent, and silent is worse than refused.
- [ ] Restore is never automatic. Backups are pulled only on explicit player action; the cloud holds the history while the local machine keeps authority over its own display.
- [ ] Export `bk_cloudsync_backup_restore(entry_id, mode) i32`, pollable like any other network call. Own the whole export path.
- [ ] Commit checkpoint: `cloudsync: restore a config backup with a GFX-preserving merge`.

**Evidence:** Unit tests show a merge preserving local `GFX.Mode`, `GFX.Monitor`, and `GFX.FullScreen` while adopting every other key, a full restore adopting all, and a staged restore surviving a simulated shutdown serialize.
