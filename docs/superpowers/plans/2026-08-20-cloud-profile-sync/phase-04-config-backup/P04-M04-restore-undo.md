# P04-M04 — pre-restore backup and undo

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make restoring itself reversible.

**Dependencies:** P04-M03.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test asserting a pre-restore copy exists and reproduces the original byte for byte.
- [ ] Before a restore writes anything, copy the current `config.cfg` to `.cloudsync-trash/config/<timestamp>.cfg`, reusing the local trash that already protects saves.
- [ ] Implement `undoRestore(allocator, ctx) !void` reinstating the most recent pre-restore copy, and export `bk_cloudsync_restore_undo() i32` plus `bk_cloudsync_restore_undo_available() u32` so the UI can offer it only when it exists. Own the whole export path.
- [ ] Write the restored config through temp-file-then-rename, so an interrupted restore cannot leave a half-written config the game then fails to parse.
- [ ] Exempt pre-restore copies from `pruneTrash` age limits up to `min_keep`; they are small and are the only undo path.
- [ ] Commit checkpoint: `cloudsync: make config restore undoable`.

**Evidence:** Unit tests show a byte-identical undo after both a merge restore and a full restore, driven through the ABI.
