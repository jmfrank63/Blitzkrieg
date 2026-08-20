# P04-M01 — config snapshot upload

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Push a copy of config.cfg after a successful sync, using none of bisync's machinery.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`.

- [ ] Write the failing test against a live daemon and a local directory standing in for the remote.
- [ ] Implement `snapshotConfig(allocator, ctx) !void` issuing a single `operations/copyfile` with `srcFs`, `srcRemote`, `dstFs`, `dstRemote`. No listings, no session state, no delete ratios — none of bisync applies here.
- [ ] Name the destination `<remote>/config-backups/<name>/<host>/<timestamp>.cfg`, with the host sanitised through the same rules as `NProfile::Sanitize`. **Note the layout: `config-backups/` is a sibling of `profiles/`, not a child of Path2.** Placed under `<remote>/profiles/<name>/` it would be inside the synced prefix and bisync would pull the whole backup history down onto every machine.
- [ ] Key by host so machines never overwrite each other's history; "restore the desktop's settings onto the laptop" only means something if both histories survive.
- [ ] Run it on the worker after a clean sync finish, and only when `Cloud.Config.Backup` is enabled.
- [ ] Assert in the test that the destination is not a descendant of Path2, and confirm `config.cfg` remains excluded from the sync filter set. It is backed up, never synced — machine-specific `GFX.Mode`, `GFX.Monitor`, and `GFX.FullScreen` are exactly what this split protects.
- [ ] Commit checkpoint: `cloudsync: snapshot config.cfg per host`.

**Evidence:** Integration output shows a snapshot at the expected per-host path, an assertion that it is not under Path2, a subsequent sync that does not pull it down, and `config.cfg` still absent from the sync set.
