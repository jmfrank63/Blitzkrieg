# P01-M01 — short link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give bisync a short Path1 so the install path never reaches the session name.

**Dependencies:** P00-M04.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test: create a profile directory nested past 190 bytes, link it, and assert the link path is under 40 bytes and lists the same contents.
- [ ] Implement `linkRoot(allocator) ![]u8` — `%LOCALAPPDATA%\bk\` on Windows, `~/Library/Caches/blitzkrieg/` on macOS, `~/.cache/blitzkrieg/` on Linux — creating it when absent.
- [ ] Implement `ensureShortLink(allocator, profile_dir) ![]u8` producing `<linkRoot>/p<slot>`, where slot is a small integer index. Do not use the profile name: a profile called `Panzerkommandant` puts the length straight back.
- [ ] On POSIX use `std.posix.symlink`. On Windows create a **junction**, not a symlink — symlinks need administrator rights or Developer Mode, junctions do not. Use `DeviceIoControl` with `FSCTL_SET_REPARSE_POINT` and an `IO_REPARSE_TAG_MOUNT_POINT` buffer, falling back to `cmd /c mklink /J` only if that fails, and record which path was taken.
- [ ] Implement `repointShortLink(allocator, slot, new_target)` for profile switching by removing and recreating. Do not assume a link can be retargeted in place.
- [ ] Prove the property the whole approach rests on: rclone does **not** resolve a symlinked root, so the short name survives into the session name. Measured on macOS (a 199-byte directory reached through an 8-byte link produced a 21-byte session name) but **unproven for Windows junctions** — prove it here or stop and report.
- [ ] Commit checkpoint: `cloudsync: short link for the profile directory`.

**Evidence:** The test shows a >190-byte profile directory reachable through a <40-byte link on the host platform, with the junction-versus-symlink path recorded.
