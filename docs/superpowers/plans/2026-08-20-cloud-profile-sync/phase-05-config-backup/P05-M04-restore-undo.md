# P05-M04 — pre-restore backup and undo

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make restoring itself reversible.

**Dependencies:** P05-M03.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`.

- [ ] Write the failing test asserting a pre-restore copy exists and reproduces the original file byte for byte.
- [ ] Before a restore writes anything, copy the current `config.cfg` into `.cloudsync-trash/config/<timestamp>.cfg`, reusing the trash that already protects saves.
- [ ] Implement `undoRestore(allocator, ctx) !void` reinstating the most recent pre-restore copy.
- [ ] Write the restored config through a temp file and rename, so an interrupted restore cannot leave a half-written config the game then fails to parse.
- [ ] Exempt pre-restore copies from `pruneTrash` age limits up to `min_keep`; they are small and are the only undo path.
- [ ] Commit checkpoint: `cloudsync: make config restore undoable`.

**Evidence:** Unit tests show a byte-identical undo after both a merge restore and a full restore.
