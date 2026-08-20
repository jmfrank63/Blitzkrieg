# P05-M02 — backup listing and retention

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player see what can be restored, and stop the history growing without bound.

**Dependencies:** P05-M01.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`.

- [ ] Write the failing test over a synthetic backup tree spanning two hosts and several timestamps.
- [ ] Implement `listBackups(allocator, client, ctx) ![]BackupEntry` over `operations/list` with `recurse`, returning `{ host, timestamp, size, remote_path }` sorted newest first.
- [ ] Implement `pruneBackups(allocator, client, ctx, keep_per_host: u32) !u32` deleting the oldest beyond the limit with `operations/deletefile`, returning how many were removed.
- [ ] Prune per host, never globally: a machine that has not run for months must not lose its history because another machine is active.
- [ ] Never prune the newest entry for any host, whatever the retention setting.
- [ ] Commit checkpoint: `cloudsync: list and prune config backups`.

**Evidence:** Unit tests show per-host retention with the newest entry always retained across both hosts.
