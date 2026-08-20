# P02-M04 — trash retention and pruning

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Keep the safety net from growing without bound, without ever pruning what protects a player.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing test over a synthetic trash tree with mixed ages.
- [ ] Place the trash at `profiles/<name>/.cloudsync-trash/` and pass it as both `backupDir1` and `backupDir2`, keeping relative paths intact — a deleted `saves/m2.sav` lands at `trash/saves/m2.sav`.
- [ ] Implement `pruneTrash(dir, opts: .{ max_age_days, min_keep })` removing entries older than the age limit but always retaining at least `min_keep` most-recent items regardless of age.
- [ ] Never prune during a sync. Run it after a clean finish only, so a failed or aborted run cannot delete the very files it may have just displaced.
- [ ] Exclude the trash from the sync filter set — it must not be uploaded — and confirm P01-M03 already does so.
- [ ] Commit checkpoint: `cloudsync: prune the profile trash`.

**Evidence:** Unit tests show age-based pruning with the `min_keep` floor honoured, and the trash absent from the filter set.
