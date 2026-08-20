# P04-M04 — pre-restore backup and undo

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make restoring itself reversible.

**Dependencies:** P04-M03.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test asserting a pre-restore copy exists and reproduces the file **as it was immediately before application**, byte for byte — including a change written between staging and startup.
- [ ] **Take the snapshot inside `apply_pending_restore`, not when the restore is staged.** Staging and application are separated by an entire session: the player can change settings, and any of nine `SerializeConfig( false, ... )` call sites can rewrite `config.cfg` in between. A copy taken at stage time therefore undoes to a file that has not existed for hours, silently discarding everything since. Copy `config.cfg` as the first step of applying, before the merged result is written.
- [ ] **Name the snapshot after the stage nonce and write it once**, `.cloudsync-trash/config/<nonce>.cfg`, skipping the copy when it already exists. A crash between the config rename and the stage cleanup makes the next launch apply the same stage again; without the nonce key that retry snapshots the already-restored file, and undo then recovers the restore rather than the original.
- [ ] Implement `undoRestore(allocator, ctx) !void` reinstating the most recent pre-restore copy **through the same staging path as the restore** — write it into `.cloudsync-restore/` as a `full`-mode stage, `COMMIT` last, and let the P06-M02 apply step install it at startup. An undo that writes `config.cfg` live is discarded by the shutdown rewrite exactly as an unstaged restore is (P04-M03). An undo restores the file as it was, so it does not merge.
- [ ] If a restore is still staged and unapplied, undo simply deletes the pending file; there is nothing to reinstate yet, and rewriting the config would be wrong.
- [ ] Export `bk_cloudsync_restore_undo() i32` and `bk_cloudsync_restore_undo_available() u32` so the UI offers undo only when a pre-restore copy or a pending file exists. Own the whole export path.
- [ ] Write the restored config through temp-file-then-rename, so an interrupted restore cannot leave a half-written config the game then fails to parse.
- [ ] Exempt pre-restore copies from `pruneTrash` age limits up to `min_keep`; they are small and are the only undo path.
- [ ] Commit checkpoint: `cloudsync: make config restore undoable`.

**Evidence:** Unit tests show a byte-identical undo after both a merge restore and a full restore across a simulated restart, an undo that recovers a change written *after* staging but before application, an undo that still recovers the original after a crash-interrupted apply is retried, and the staged-but-unapplied case where undo discards the stage.
