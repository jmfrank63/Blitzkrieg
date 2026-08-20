# P00-M02 — rclone discovery and version gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Locate a usable rclone binary and refuse one too old for the rc surface this plan needs.

**Dependencies:** P00-M01.

**Allowed files:** `Sources/src/CloudSync/daemon.zig`, `Sources/src/CloudSync/daemon_test.zig`, `build.zig`.

- [ ] Write the failing test over a fake PATH in a temp directory holding a stub executable that prints a chosen version string.
- [ ] Implement `discover(allocator, explicit: ?[]const u8) !?[]const u8` searching in order: the explicit path from `cloud.credentials`, the game directory, then `PATH`. Return null rather than erroring when nothing is found — absence disables the feature, it is not a failure.
- [ ] Implement `probeVersion(allocator, path) !Version` running `<rclone> version` and parsing the leading `vMAJOR.MINOR.PATCH`.
- [ ] Gate on `MIN_RCLONE = .{ .major = 1, .minor = 66 }`: this plan depends on `resyncMode`, on `backupDir1`/`backupDir2` as rc parameters, and on the non-suffixed listing filenames introduced when `bilib.BasePath` began renaming the older `{hexstring}` form.
- [ ] Return a typed reason on rejection — `.not_found`, `.too_old`, `.not_executable` — so the settings UI can say which rather than showing a bare unavailable state.
- [ ] Commit checkpoint: `cloudsync: locate and version-gate the rclone binary`.

**Evidence:** `zig build test-cloudsync-daemon` passes discovery order, a too-old rejection, and the not-found path.
