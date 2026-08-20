# P04-M02 — backup listing and retention

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player see what can be restored, and stop the history growing without bound.

**Dependencies:** P04-M01.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test over a synthetic backup tree spanning two hosts and several timestamps.
- [ ] Implement `listBackups(allocator, ctx) ![]BackupEntry` over `operations/list` with `recurse`, returning `{ id, host, timestamp, size, remote_path }` sorted newest first, where `id` is a stable opaque handle the UI can pass back.
- [ ] Implement `pruneBackups(allocator, ctx, keep_per_host: u32) !u32` deleting the oldest beyond the limit with `operations/deletefile`, returning how many were removed.
- [ ] Prune per host, never globally: a machine idle for months must not lose its history because another machine is busy. Never prune the newest entry for any host, whatever the retention setting.
- [ ] Export the listing so a UI can exist: `bk_cloudsync_backup_list(handle_out) i32` starting the (networked, therefore pollable) fetch, and `bk_cloudsync_backup_entry(handle, index, json_out, cap) i32` reading one entry. Own the whole export path.
- [ ] Commit checkpoint: `cloudsync: list and prune config backups`.

**Evidence:** Unit tests show per-host retention with the newest entry always retained, and the listing readable from C++ through the ABI test.
