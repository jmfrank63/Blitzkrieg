# P04-M04 — pre-restore backup and undo

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make restoring itself reversible.

**Dependencies:** P04-M03.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test asserting a pre-restore copy exists and reproduces the original byte for byte.
- [ ] Before a restore writes anything, copy the current `config.cfg` to `.cloudsync-trash/config/<timestamp>.cfg`, reusing the local trash that already protects saves.
- [ ] Implement `undoRestore(allocator, ctx) !void` reinstating the most recent pre-restore copy **through the same staging path as the restore** — write it to `config.cfg.pending-restore` with a mode marker of `full`, and let `P06-M02` apply it at startup. An undo that writes `config.cfg` live is discarded by the shutdown rewrite exactly as an unstaged restore is (P04-M03). An undo restores the file as it was, so it does not merge.
- [ ] If a restore is still staged and unapplied, undo simply deletes the pending file; there is nothing to reinstate yet, and rewriting the config would be wrong.
- [ ] Export `bk_cloudsync_restore_undo() i32` and `bk_cloudsync_restore_undo_available() u32` so the UI offers undo only when a pre-restore copy or a pending file exists. Own the whole export path.
- [ ] Write the restored config through temp-file-then-rename, so an interrupted restore cannot leave a half-written config the game then fails to parse.
- [ ] Exempt pre-restore copies from `pruneTrash` age limits up to `min_keep`; they are small and are the only undo path.
- [ ] Commit checkpoint: `cloudsync: make config restore undoable`.

**Evidence:** Unit tests show a byte-identical undo after both a merge restore and a full restore across a simulated restart, plus the staged-but-unapplied case where undo discards the pending file.
